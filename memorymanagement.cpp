#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Choice.H>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cmath>
#include <stdexcept>

using namespace std;

struct page {
    int page_no;
    int validBit;
    unsigned long last_Access;
};

int NUM_ADDRESSES = 512;
unsigned long Access_time = 1;
int Cur_page_no = 0;
map<int, int> psizes;
map<int, int> access_counter;
int faultCounter = 0;
vector<page**> table;
int Pages_no;
page* mainMemory[512];
int clock_Ind = 0;

int find_MemoryLoc(int pid, int loc, int pagesize) {
    if (loc < 0 || loc > psizes[pid]) { 
        throw std::invalid_argument("ERROR!! Invalid Argument! the received is out of the allowed");
    }
    int res = (int)ceil((float)loc / (float)pagesize) - 1;
    return res;
}

void reset_simulation(Fl_Output* output_faults) {
    Access_time = 1;
    Cur_page_no = 0;
    psizes.clear();
    access_counter.clear();
    faultCounter = 0;
    table.clear();
    clock_Ind = 0;
    output_faults->value("0");
    for (int i = 0; i < 512; i++) {
        mainMemory[i] = nullptr;
    }
}

void start_simulation(Fl_Widget *widget, void *data) {
    auto inputs = static_cast<tuple<Fl_Input*, Fl_Choice*, Fl_Output*>*>(data);
    Fl_Input* input_page_size = get<0>(*inputs);
    Fl_Choice* input_algorithm = get<1>(*inputs);
    Fl_Output* output_faults = get<2>(*inputs);


    reset_simulation(output_faults);

    int page_size = atoi(input_page_size->value());

    const char* algorithm = input_algorithm->text();
    int algCase = -1;
    if (strcmp(algorithm, "FIFO") == 0) {
        algCase = 0;
    } else if (strcmp(algorithm, "LRU") == 0) {
        algCase = 1;
    } else if (strcmp(algorithm, "Clock") == 0) {
        algCase = 2;
    } else if (strcmp(algorithm, "LFU") == 0) {
        algCase = 3;
    } else {
        cout << "ERROR!! Invalid parameter please try again later" << endl;
        return;
    }


    bool prePaging = false;  
    bool notYetPrepaged = true;

    ifstream plist("plist.txt");
    char c;
    char lastChar = '\n';
    bool readPID = false;
    string currPID = "";
    int pid = 0;
    string currSize = "";
    int size = 0;

    while (plist.get(c)) { 
        if (c == '\n' && lastChar != '\n') { 
            size = atoi(currSize.c_str()); 
            pid = atoi(currPID.c_str());
            psizes[pid] = size;
            int Pages_no = (int)ceil((float)size / (float)page_size);
            page** anythingreally = new page*[Pages_no];
            for (int i = 0; i < Pages_no; i++) {
                anythingreally[i] = new page;
                anythingreally[i]->page_no = Cur_page_no++;
                anythingreally[i]->validBit = 0;
                anythingreally[i]->last_Access = 0;
            }
            table.push_back(anythingreally);
            currSize.clear();
            currPID.clear();
            readPID = false;
        } else if (!readPID && c != ' ') {
            currPID += c;
        } else if (c == ' ') {
            readPID = true;
        } else {
            currSize += c;
        }
        lastChar = c;
    }
    plist.close();

    Pages_no = NUM_ADDRESSES / page_size;
    int pagesPerProgram = Pages_no / table.size();
    for (int i = 0; i < table.size(); ++i) {
        for (int j = 0; j < pagesPerProgram; ++j) {
            mainMemory[j + pagesPerProgram * i] = table[i][j];
            table[i][j]->validBit = 1;
            table[i][j]->last_Access = Access_time++;
        }
    }

    Pages_no = pagesPerProgram * table.size();

    ifstream ptrace("ptrace.txt");
    lastChar = '\n';
    readPID = false;
    currPID.clear();
    pid = 0;
    currSize.clear();
    size = 0;
    int indexToSwap = 0;
    int tempsize;

    while (ptrace.get(c)) {
        if (c == '\n' && lastChar != '\n') {
            size = atoi(currSize.c_str());
            pid = atoi(currPID.c_str());
            if (table[pid][find_MemoryLoc(pid, size, page_size)]->validBit == 0) {
                faultCounter++;
                prepagecase:
                if (algCase == 0) {  
                    unsigned long minTime = Access_time;
                    int minIndex = 0;
                    for (int i = 0; i < Pages_no; ++i) {
                        if (minTime > mainMemory[i]->last_Access) {
                            minIndex = i;
                            minTime = mainMemory[i]->last_Access;
                        }
                    }
                    indexToSwap = minIndex;
                } else if (algCase == 1) { 
                    unsigned long minTime = Access_time;
                    int minIndex = 0;
                    for (int i = 0; i < Pages_no; ++i) {
                        if (minTime > mainMemory[i]->last_Access) {
                            minIndex = i;
                            minTime = mainMemory[i]->last_Access;
                        }
                    }
                    indexToSwap = minIndex;
                } else if (algCase == 2) {  
                    while (mainMemory[clock_Ind]->validBit == 1) {
                        mainMemory[clock_Ind]->validBit = 2;
                        clock_Ind = (clock_Ind + 1) % Pages_no;
                    }
                    indexToSwap = clock_Ind;
                    clock_Ind = (clock_Ind + 1) % Pages_no;
                } else if (algCase == 3) {  
                    int minCount = access_counter[mainMemory[0]->page_no];
                    int minIndex = 0;
                    for (int i = 0; i < Pages_no; ++i) {
                        if (access_counter[mainMemory[i]->page_no] < minCount) {
                            minIndex = i;
                            minCount = access_counter[mainMemory[i]->page_no];
                        }
                    }
                    indexToSwap = minIndex;
                }
                mainMemory[indexToSwap]->validBit = 0;
                int checkPageNum = mainMemory[indexToSwap]->page_no;
                if (notYetPrepaged) {
                    mainMemory[indexToSwap] = table[pid][find_MemoryLoc(pid, size, page_size)];
                    mainMemory[indexToSwap]->validBit = 1;
                    mainMemory[indexToSwap]->last_Access = Access_time++;
                } else {
                    mainMemory[indexToSwap] = table[pid][find_MemoryLoc(pid, tempsize, page_size)];
                    mainMemory[indexToSwap]->validBit = 1;
                    mainMemory[indexToSwap]->last_Access = Access_time++;
                }
                if (prePaging && notYetPrepaged) {
                    notYetPrepaged = false;
                    tempsize = (size + page_size) % psizes[pid];
                    if (tempsize > size && table[pid][find_MemoryLoc(pid, tempsize, page_size)]->page_no == checkPageNum) {
                        tempsize = (tempsize + page_size) % psizes[pid];
                    }
                    while (tempsize > size && table[pid][find_MemoryLoc(pid, tempsize, page_size)]->validBit == 1) {
                        tempsize = (tempsize + page_size) % psizes[pid];
                        if (tempsize > size && table[pid][find_MemoryLoc(pid, tempsize, page_size)]->page_no == checkPageNum) {
                            tempsize = (tempsize + page_size) % psizes[pid];
                        }
                    }
                    if (tempsize > size) {
                        goto prepagecase;
                    }
                }
            } else if (algCase == 1) {  
                table[pid][find_MemoryLoc(pid, size, page_size)]->last_Access = Access_time++;
            } else if (table[pid][find_MemoryLoc(pid, size, page_size)]->validBit == 2) {
                table[pid][find_MemoryLoc(pid, size, page_size)]->validBit = 1;
            }
            notYetPrepaged = true;
            currSize.clear();
            currPID.clear();
            readPID = false;
        } else if (!readPID && c != ' ') {
            currPID += c;
        } else if (c == ' ') {
            readPID = true;
        } else {
            currSize += c;
        }
        lastChar = c;
    }
    ptrace.close();
    
    char faults_str[10];
    sprintf(faults_str, "%d", faultCounter);
    output_faults->value(faults_str);
}

int main(int argc, char **argv) {
    Fl_Window *window = new Fl_Window(400, 350, "Virtual Memory Simulator GUI");
    
    Fl_Input *input_page_size = new Fl_Input(150, 30, 200, 30, "Page Size:");
    input_page_size->value("16");

    Fl_Choice *choice_algorithm = new Fl_Choice(150, 80, 200, 30, "Algorithm:");
    choice_algorithm->add("FIFO");
    choice_algorithm->add("LRU");
    choice_algorithm->add("Clock");
    choice_algorithm->add("LFU");
    choice_algorithm->value(0); 

    Fl_Button *button_start = new Fl_Button(150, 130, 200, 30, "Starting the Simulation");

    Fl_Output *output_faults = new Fl_Output(150, 180, 200, 30, "Page Faults:");
    output_faults->value("0");

    tuple<Fl_Input*, Fl_Choice*, Fl_Output*> data(input_page_size, choice_algorithm, output_faults);
    button_start->callback(start_simulation, &data);

    Fl_Button *button_reset = new Fl_Button(150, 230, 200, 30, "Reseting the Simulation");
    button_reset->callback([](Fl_Widget*, void* data) { 
        auto output_faults = static_cast<Fl_Output*>(data);
        reset_simulation(output_faults);
    }, output_faults);

    window->end();
    window->show(argc, argv);
    return Fl::run();
}
