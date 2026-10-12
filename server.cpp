// ======================= TIME-TRAVEL DEBUGGER - SERVER TEMPLATE =======================

// Pipeline this file implements, top to bottom:
//   0. Receive  -- stream the client's .trace bytes straight to source.bin on disk
//   1. Pass 0X0   -- validity check (FUNC/FUNC_END matching)
//   2. Pass 0X1   -- resolve(): copy EVERY source line into resolve.bin as [offset][size][string], then patch CALL targets.
//   3. Pass 0X2   -- execute resolve.bin: tokenize ONE line at a time, update the call stack, take a snapshot -> Timeline
//   4. Pass 0X3   -- serialize Timeline -> session.tdbg(header + snapshot records + dense index)


#include <iostream>
#include <string>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <cstdint>
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#endif

using namespace std;

// ---- Constants ----
const int32_t MAX_VARS_PER_FRAME = 16;
const int32_t MAX_STACK_DEPTH = 64;
const int32_t MAX_FUNCS = 128;
const int32_t MAX_TOKENS = MAX_VARS_PER_FRAME + 2; // kW + func_name + upto 16 params/args
const int32_t MAX_PATCHES = MAX_FUNCS * 4;
const uint64_t MAX_SOURCE_BYTES = 15ULL * 1024 * 1024; // sanity cap on the declared file length
const int32_t IO_BUFFER_SIZE = 64 * 1024;                  // fixed buffer for streaming to/from disk
const int32_t SOCKET_TIMEOUT_SEC = 5;                      // TODO: apply as SO_RCVTIMEO so a deadclient can't hang the server forever

// ---- Custom data structures

// Stack: back the live Call Stack during execution
template <typename T>
class Stack
{
    struct Node
    {
        T data;
        Node *next;
    };
    Node *top;
    int32_t count;

public:
    // Implement these functions:
    Stack()
    { // initialize the stack
        top = NULL;
        count = 0;
    }
    void push(const T &val)
    {
    if(count >= MAX_STACK_DEPTH){
    cout<<"LIMIT REACHED"<<endl;
    return;
    }
    Node* temp = new Node;

    temp->data = val;
    temp->next = top;
    top = temp;
    count++;


        
    }
    T pop()
    {
        if(top==NULL){
            cout<<"Empty Stack"<<endl;
            return T();
        }
        Node* temp = top;
        T val = temp->data;

        top = top->next;
        delete temp;
        count--;

        return val;
    }
    T &peek()
    {
        if (top == NULL)
        {
            throw "Stack is empty";
        }

        return top->data;
    }
    bool isEmpty()
    {
        return top == NULL;
    }
    int32_t depth()
    {
        return count;
    }
    int32_t snapshot_into(T out[], int32_t maxLen)
    {
       
        Node *temp = top;
        int32_t written = 0;

        while (temp != NULL && written < maxLen)
        {
            out[written] = temp->data;
            written++;
            temp = temp->next;
        }

        return written;

    }
};


// Timeline : doubly linked list of Snapshots
struct Snapshot; // fwd declaration;
struct TimelineNode
{
    Snapshot *data;
    TimelineNode *next;
    TimelineNode *prev;
};
class Timeline
{
    TimelineNode *head, *tail;
    int32_t stepCount;

public:
    // Implement these functions
    Timeline()
    {
        head = NULL;
        tail = NULL;
        stepCount=0;
    }
    void record(Snapshot *s)
    { 
        TimelineNode * temp = new TimelineNode;
        temp->data = s;
        temp->next = NULL;
        temp->prev = tail;

        if(head==NULL){
            head= temp;
            tail=temp;
        }

        else{
            tail->next= temp;
            tail=temp;
        }

        stepCount++;
        
    }
    TimelineNode *begin()
    {
        return head;
    }
    int32_t getStepCount()
    {
        return stepCount;
    }
};

// Core structs
struct Variable
{
    string name;
    int32_t value;
};
struct Frame
{
    string func_name;
    int32_t argc;
    Variable argv[MAX_VARS_PER_FRAME];
    int32_t returnLine;
    Variable locals[MAX_VARS_PER_FRAME];
    int32_t localCount;
};
struct Snapshot
{
    Frame callStack[MAX_STACK_DEPTH];
    int32_t stackDepth;
};
struct TTDBHeader
{
    char magic[4]; // "TTDB"
    int32_t version;
    int32_t stepCount;
    int64_t indexOffset;
};
void writeHeader(FILE *f, const TTDBHeader &h)
{
    fwrite(h.magic, 1, 4, f);
    fwrite(&h.version, sizeof(int32_t), 1, f);
    fwrite(&h.stepCount, sizeof(int32_t), 1, f);
    fwrite(&h.indexOffset, sizeof(int64_t), 1, f);
}

void writeString(FILE *f, const string &text)
{
    int32_t length = (int32_t)text.length();
    fwrite(&length, sizeof(int32_t), 1, f);
    fwrite(text.c_str(), 1, length, f);
}

void writeVariable(FILE *f, const Variable &var)
{
    writeString(f, var.name);
    fwrite(&var.value, sizeof(int32_t), 1, f);
}

void writeFrame(FILE *f, const Frame &frame)
{
    writeString(f, frame.func_name);

    fwrite(&frame.argc, sizeof(int32_t), 1, f);

    for (int32_t i = 0; i < frame.argc; i++)
    {
        writeVariable(f, frame.argv[i]);
    }

    fwrite(&frame.returnLine, sizeof(int32_t), 1, f);

    fwrite(&frame.localCount, sizeof(int32_t), 1, f);

    for (int32_t i = 0; i < frame.localCount; i++)
    {
        writeVariable(f, frame.locals[i]);
    }
}

void writeSnapshot(FILE *f, const Snapshot &snap)
{
    fwrite(&snap.stackDepth, sizeof(int32_t), 1, f);

    for (int32_t i = 0; i < snap.stackDepth; i++)
    {
        writeFrame(f, snap.callStack[i]);
    }
}

// resolve.bin - bookkeeping
struct FuncEntry
{
    string funcName;
    int64_t byteOffsetInResolveBin; // where this function's FUNC header record sits
};
struct PendingPatch
{
    int64_t byteOffsetOfOffsetField; // where in resolve.bin to seek back and overwrite
    string targetFuncName;
};



// PASS 0x0: READING source.bin + VALIDITY CHECK
bool readSourceLine(ifstream &in, string &out)
{
while(getline(in,out)){
    if(out != ""){
        return true;
    }
}
return false;

    // reads the next nonblank line
}
string firstWord(const string &line)
{
    size_t i = 0;
    string word = "";

    while (i < line.length() && line[i] == ' ')
    {
        i++;
    }

    while (i < line.length() && line[i] != ' ')
    {
        word += line[i];
        i++;
    }

    return word;
}
string secondWord(const string &line)
{
    size_t i=0;

    string word="";
    int sp=0;

    while(i<line.length()&& sp<2){
        if(line[i]==' '){

       sp++;

       while(i<line.length()&& line[i]==' '){
        i++;
       }
        }

        else{

            if(sp==1){
                word=word+line[i];
            }
            i++;
        }
    }
    return word;
}

bool validateProgram(const char *sourcePath)
{
    ifstream in(sourcePath);

    if (!in)
    {
        cout << "Could not open source.bin" << endl;
        return false;
    }

    bool insideFunc = false;
    string line;

    while (readSourceLine(in, line))
    {
        string word = firstWord(line);

        if (word == "func")
        {
            if (insideFunc)
            {
                cout << "Invalid program: nested functions not allowed" << endl;
                return false;
            }

            insideFunc = true;
        }
        else if (word == "func_end")
        {
            if (!insideFunc)
            {
                cout << "Invalid program: unmatched func_end" << endl;
                return false;
            }

            insideFunc = false;
        }
    }

    in.close();

    if (insideFunc)
    {
        cout << "Invalid program: missing func_end" << endl;
        return false;
    }

    return true;
}

// PASS 0x1: RESOLVE() -> resolve.bin
int64_t writeResolveRecord(FILE *f, int64_t offsetField, const string &text)
{
    int64_t recordPosition = ftell(f);
    int32_t size = (int32_t)text.size();

    if (recordPosition < 0){
        return -1;

    }

    if (fwrite(&offsetField, sizeof(int64_t), 1, f) != 1)  {

        return -1;
    }

    if (fwrite(&size, sizeof(int32_t), 1, f) != 1) {

        return -1;
    }

    if (size > 0)    {


        if (fwrite(text.c_str(), 1, size, f) != (size_t)size){ 

            return -1;
        }
    }

    return recordPosition;
}
int64_t readResolveRecord(FILE *f, string &outText)
{
    int64_t offsetField;
    int32_t size;

    if (fread(&offsetField, sizeof(int64_t), 1, f) != 1){

    
    
        return -1;
    }

    if (fread(&size, sizeof(int32_t), 1, f) != 1){

    
        return -1;
    }

    if (size < 0 || size > MAX_SOURCE_BYTES){
    
        return -1;
    }

    char *buffer = new char[size + 1];

    if (size > 0){
    
        if (fread(buffer, 1, size, f) != (size_t)size){
        
            delete[] buffer;
            return -1;
        }
    }

    buffer[size] = '\0';
    outText = buffer;

    delete[] buffer;

    return offsetField;
}

int64_t resolveProgram(const char *sourcePath, const char *resolveBinPath)
{
    FuncEntry funcArray[MAX_FUNCS];
    int32_t funcCount = 0;
    PendingPatch patches[MAX_PATCHES];
    int32_t patchCount = 0;

    ifstream in(sourcePath);

    if (!in)  {
        cout << "Could not open source file" << endl;
        return -1;
    }

    FILE *out = fopen(resolveBinPath, "wb");

    if (out == NULL) {
        cout << "Could not create resolve.bin" << endl;
        in.close();
        return -1;
    }

    string line;
    int64_t mainOffset = -1;
    bool valid = true;

    while (getline(in, line)) {
        string word = firstWord(line);
        int64_t pos = ftell(out);

        if (pos < 0)   {
            valid = false;
            break;
        }

        if (word == "func")  {
            string name = secondWord(line);

            if (name == "") {
                cout << "Invalid function declaration" << endl;
                valid = false;
                break;
            }

            for (int32_t i = 0; i < funcCount; i++)  {
                if (funcArray[i].funcName == name) {
                    cout << "Duplicate function: " << name << endl;
                    valid = false;
                    break;
                }
            }

            if (!valid) {
                break;
            }

            if (funcCount >= MAX_FUNCS)  {
                cout << "Too many functions" << endl;
                valid = false;
                break;
            }

            funcArray[funcCount].funcName = name;
            funcArray[funcCount].byteOffsetInResolveBin = pos;
            funcCount++;

            if (name == "main")  {
                mainOffset = pos;
            }
        }

        if (word == "call")  {
            string name = secondWord(line);

            if (name == "")    {
                cout << "Invalid call instruction" << endl;
                valid = false;
                break;
            }

            if (patchCount >= MAX_PATCHES)    {
                cout << "Too many function calls" << endl;
                valid = false;
                break;
            }

            patches[patchCount].byteOffsetOfOffsetField = pos;
            patches[patchCount].targetFuncName = name;
            patchCount++;
        }

        if (writeResolveRecord(out, pos, line) < 0)  {
            cout << "Could not write resolve.bin" << endl;
            valid = false;
            break;
        }
    }

    in.close();

    if (valid && mainOffset == -1) {
        cout << "Invalid program: main function not found" << endl;
        valid = false;
    }

    for (int32_t i = 0; i < patchCount && valid; i++) {
        bool found = false;
        int64_t target = -1;

        for (int32_t j = 0; j < funcCount; j++)  {
            if (funcArray[j].funcName == patches[i].targetFuncName) {

                target = funcArray[j].byteOffsetInResolveBin;
                found = true;
                break;
            }
        }

        if (!found) {
            cout << "Undefined function: "
                 << patches[i].targetFuncName << endl;
            valid = false;
            break;
        }

        if (fseek(out, (long)patches[i].byteOffsetOfOffsetField, SEEK_SET) != 0)  {


            cout << "Could not find call position" << endl;
            valid = false;
            break;
        }

        if (fwrite(&target, sizeof(int64_t), 1, out) != 1)  {
            cout << "Could not update call position" << endl;
            valid = false;
            break;
        }
    }

    fclose(out);

    if (!valid) {

        remove(resolveBinPath);
        return -1;
    }

    return mainOffset;
}

// PASS 0x2: EXECUTION (tokenization happens here)
enum TokenType
{
    KEYWORD,
    IDENTIFIER,
    PARAM
};
struct Token
{
    TokenType type;
    string text;
};
int32_t tokenizeLine(const string &line, Token tokens[], int32_t maxTokens)
{
    // first word is always a instruction keyword
    // instruction set = [func, func_end, call, set, add, sub, mul and div]
    // next word is identifier like name of a function, variable name
    // after identifier all are the params/arg, space separated
    int32_t count = 0;
    string word = "";

    for (int32_t i = 0; i < (int32_t)line.length(); i++) {

        if (line[i] != ' ') {

            word += line[i];

        } 
        
        else if (word != "") {

            tokens[count].text = word;

            if (count == 0) {
                tokens[count].type = KEYWORD;

            } 
            else if (count == 1) {
                tokens[count].type = IDENTIFIER;

            } 
            else {
                tokens[count].type = PARAM;
            }

            count++;
            word = "";

            if (count >= maxTokens) {
                return count;

            }
        }

    }

    if (word != "" && count < maxTokens) {

        tokens[count].text = word;

        if (count == 0) {
            tokens[count].type = KEYWORD;

        } 
        else if (count == 1) {
            tokens[count].type = IDENTIFIER;

        } 
        else {
            tokens[count].type = PARAM;

        }

        count++;
    }

    return count;

}
Snapshot *buildSnapshot(Stack<Frame> &callStack)
{
    Snapshot * snap = new Snapshot;

    snap->stackDepth = callStack.snapshot_into(snap->callStack,MAX_STACK_DEPTH);
    return snap;
}
int32_t parseInteger(string text)
{
    int32_t number = 0;
    int32_t sign = 1;
    int32_t i = 0;

    if (text[0] == '-'){
        sign = -1;
        i = 1;
    }

    for (; i < (int32_t)text.length(); i++) {
        number = number * 10 + (text[i] - '0');
    }

    return number * sign;
}

int32_t getFrameValue(Frame &frame, string name)
{
    for (int32_t i = 0; i < frame.argc; i++)  {

        if (frame.argv[i].name == name) {

            return frame.argv[i].value;
        }
    }

    for (int32_t i = 0; i < frame.localCount; i++) {


        if (frame.locals[i].name == name) {
            return frame.locals[i].value;
        }
    }

    if (frame.localCount < MAX_VARS_PER_FRAME) {


        frame.locals[frame.localCount].name = name;
        frame.locals[frame.localCount].value = 0;
        frame.localCount++;
    }

    return 0;
}

void setFrameValue(Frame &frame, string name, int32_t value)
{
    for (int32_t i = 0; i < frame.argc; i++)
    {
        if (frame.argv[i].name == name) {


            frame.argv[i].value = value;
            return;
        }
    }

    for (int32_t i = 0; i < frame.localCount; i++)
    {
        if (frame.locals[i].name == name) {

            frame.locals[i].value = value;
            return;
        }
    }

    if (frame.localCount < MAX_VARS_PER_FRAME) {


        frame.locals[frame.localCount].name = name;
        frame.locals[frame.localCount].value = value;
        frame.localCount++;
    }
}

int32_t getOperandValue(Frame &frame, string text)
{
    if (text[0] == '-' || (text[0] >= '0' && text[0] <= '9')) {

        return parseInteger(text);
    }

    return getFrameValue(frame, text);
}

void executeProgram(const char *resolveBinPath, int64_t mainOffset, Timeline &timeline)
{



    FILE *in = fopen(resolveBinPath, "rb");

    if (in == NULL) {

        cout << "Could not open resolve.bin" << endl;
        return;
    }

    Stack<Frame> callStack;
    string actualArgs[MAX_STACK_DEPTH][MAX_VARS_PER_FRAME];
    string line;
    Token tokens[MAX_TOKENS];

    fseek(in, (long)mainOffset, SEEK_SET);

    readResolveRecord(in, line);

    int32_t count = tokenizeLine(line, tokens, MAX_TOKENS);

    Frame mainFrame = {};
    mainFrame.func_name = tokens[1].text;
    mainFrame.argc = 0;
    mainFrame.localCount = 0;
    mainFrame.returnLine = 0;

    callStack.push(mainFrame);
    timeline.record(buildSnapshot(callStack));

    while (!callStack.isEmpty())  {


        line = "";
        int64_t targetOffset = readResolveRecord(in, line);

        if (feof(in)) {

            break;
        }

        long returnPosition = ftell(in);
        count = tokenizeLine(line, tokens, MAX_TOKENS);

        if (count == 0) {

            continue;
        }

        string keyword = tokens[0].text;

        if (keyword == "func_end")  {

            Frame finishedFrame = callStack.pop();

            if (callStack.isEmpty()) {

                timeline.record(buildSnapshot(callStack));
                break;
            }

            Frame &caller = callStack.peek();

            for (int32_t i = 0; i < finishedFrame.argc; i++) {

                setFrameValue(caller, actualArgs[callStack.depth()][i],
                              finishedFrame.argv[i].value);
            }

            fseek(in, finishedFrame.returnLine, SEEK_SET);
            timeline.record(buildSnapshot(callStack));
            continue;
        }

        if (keyword == "call")  {

            if (callStack.depth() >= MAX_STACK_DEPTH) {


                cout << "Maximum call stack depth reached" << endl;
                break;
            }

            Frame &caller = callStack.peek();
            Frame calledFrame = {};

            calledFrame.returnLine = (int32_t)returnPosition;
            calledFrame.localCount = 0;

            fseek(in, (long)targetOffset, SEEK_SET);

            string functionLine;
            readResolveRecord(in, functionLine);

            Token functionTokens[MAX_TOKENS];
            int32_t functionCount = tokenizeLine(
                functionLine, functionTokens, MAX_TOKENS);

            if (functionCount < 2) {


                cout << "Invalid function call" << endl;
                break;
            }

            calledFrame.func_name = functionTokens[1].text;
            calledFrame.argc = functionCount - 2;

            for (int32_t i = 0; i < calledFrame.argc; i++)  
            {
                calledFrame.argv[i].name = functionTokens[i + 2].text;
                calledFrame.argv[i].value = 0;

                if (i + 2 < count){


                    actualArgs[callStack.depth()][i] = tokens[i + 2].text;
                    calledFrame.argv[i].value =
                        getFrameValue(caller, tokens[i + 2].text);
                }
            }

            callStack.push(calledFrame);
            timeline.record(buildSnapshot(callStack));
            continue;
        }

        Frame &frame = callStack.peek();

        if (keyword == "set" && count >= 3) {


            int32_t value = getOperandValue(frame, tokens[2].text);
            setFrameValue(frame, tokens[1].text, value);
        }
        else if (keyword == "add" && count >= 3) {


            int32_t value = getOperandValue(frame, tokens[2].text);
            int32_t oldValue = getFrameValue(frame, tokens[1].text);
            setFrameValue(frame, tokens[1].text, oldValue + value);

        }
        else if (keyword == "sub" && count >= 3) {

            int32_t value = getOperandValue(frame, tokens[2].text);
            int32_t oldValue = getFrameValue(frame, tokens[1].text);
            setFrameValue(frame, tokens[1].text, oldValue - value);
        }
        else if (keyword == "mul" && count >= 3)  {

            int32_t value = getOperandValue(frame, tokens[2].text);
            int32_t oldValue = getFrameValue(frame, tokens[1].text);
            setFrameValue(frame, tokens[1].text, oldValue * value);
        }
        else if (keyword == "div" && count >= 3) {

            int32_t value = getOperandValue(frame, tokens[2].text);

            if (value != 0) { 

                int32_t oldValue = getFrameValue(frame, tokens[1].text);
                setFrameValue(frame, tokens[1].text, oldValue / value);
            }
            else {

                cout << "Cannot divide by zero" << endl;
                break;
            }
        }

        timeline.record(buildSnapshot(callStack));
    }

    fclose(in);
}

// PASS 0x3: SERIALIZE TIMELINE
void writeTdbg(Timeline &timeline, const char *tdbgPath)
{


    FILE *out = fopen(tdbgPath, "wb");

    if (out == NULL)
    {
        cout << "Could not create session.tdbg" << endl;
        return;
    }

    TTDBHeader header;

    header.magic[0] = 'T';
    header.magic[1] = 'T';
    header.magic[2] = 'D';
    header.magic[3] = 'B';
    header.version = 1;
    header.stepCount = timeline.getStepCount();
    header.indexOffset = 0;

    writeHeader(out, header);

    int32_t stepCount = timeline.getStepCount();
    int64_t *index = new int64_t[stepCount];

    TimelineNode *current = timeline.begin();

    for (int32_t i = 0; i < stepCount; i++)
    {
        index[i] = (int64_t)ftell(out);

        writeSnapshot(out, *current->data);

        current = current->next;
    }

    header.indexOffset = (int64_t)ftell(out);

    for (int32_t i = 0; i < stepCount; i++)
    {
        fwrite(&index[i], sizeof(int64_t), 1, out);
    }

    fseek(out, 0, SEEK_SET);
    writeHeader(out, header);

    delete[] index;
    fclose(out);
}




// main section
int32_t main()
{

    if (!validateProgram("source.bin"))
    {
        // send an error response instead of a .tdbg file
        return 1;
    }

 int64_t mainOffset = resolveProgram("source.bin", "resolve.bin");

if (mainOffset < 0) {

    remove("session.tdbg");
    return 1;
}

Timeline timeline;
executeProgram("resolve.bin", mainOffset, timeline);

writeTdbg(timeline, "session.tdbg");

return 0;
}
