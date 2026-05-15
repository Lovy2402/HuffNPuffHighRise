#include <stdio.h>
#include <algorithm>
#include <math.h>
#include <vector>
#include <stdlib.h>
#include <random>
#include <iostream>
#include<chrono>

using namespace std;

std::random_device rd;
std::mt19937 rng(
    std::chrono::high_resolution_clock::now()
        .time_since_epoch()
        .count()
);

enum Symbol{
    HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4, WILD,HAT,EXPHAT,HORHAT,VERTHAT,BLANK
};

const char* SymbolChar[] =
{
    "HV1",
    "HV2",
    "HV3",
    "HV4",
    "LV1",
    "LV2",
    "LV3",
    "LV4",
    "WILD",
    "HAT"
};

const Symbol symbolArray[] = {HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4};
int no_of_symbols = sizeof(symbolArray)/sizeof(symbolArray[0]);
const int SYMBOL_COUNT = 9;
constexpr int MAX_MATCH = 6; // use index 0..5
const int no_of_reels = 5; 
const int no_of_rows = 6;
const int bet = 20;

int PayTable[SYMBOL_COUNT][MAX_MATCH] =
{
    // 0x  1x  2x  3x   4x    5x

    { 0,  0,  0,  20,  100,  500 },   // HV1
    { 0,  0,  0,  15,  80,   400 },   // HV2
    { 0,  0,  0,  10,  60,   300 },   // HV3
    { 0,  0,  0,  8,   40,   200 },   // HV4

    { 0,  0,  0,  5,   25,   100 },   // LV1
    { 0,  0,  0,  5,   20,   80  },   // LV2
    { 0,  0,  0,  3,   15,   60  },   // LV3
    { 0,  0,  0,  3,   10,   50  },   // LV4

    { 0,  0,  0,  0,   0,    0 }      // HAT / special
};

long long int HitTable[SYMBOL_COUNT][MAX_MATCH] = {};
long long int WinTable[SYMBOL_COUNT][MAX_MATCH] = {};


enum Symbol BG1_reel1[] = {LV1,LV2,HV1,LV4,HAT,LV3,HV2,LV1,LV2,HV3,EXPHAT,LV4,LV1,HV1,HAT,LV3,HV4,LV2};
enum Symbol BG1_reel2[] = {HV4,LV1,HV1,LV2,LV3,HAT,HV2,LV4,WILD,LV1,HV3,LV2,LV4,HV1,LV3,HV2};
enum Symbol BG1_reel3[] ={LV4,HV4,LV1,HV1,HAT,LV2,HV3,WILD,LV3,HV2,LV1,LV4,HV1,LV2,HV4,LV3,HAT,LV1,HV2,LV4};
enum Symbol BG1_reel4[] ={LV2,HV2,LV4,HAT,LV3,HV1,WILD,LV1,HV3,LV2,LV4,HV2,LV3,HV4,LV1,HAT,LV2};
enum Symbol BG1_reel5[] ={HV4,LV2,HV3,LV1,HAT,LV4,HV1,LV3,WILD,LV2,HV2,LV1,LV4,HV3,LV2,HAT,HV1,LV3,HV4};

enum Symbol* BG1_Reels[] = {BG1_reel1,BG1_reel2,BG1_reel3,BG1_reel4,BG1_reel5};
vector<int> BG1_Reelsize = {
    sizeof(BG1_reel1)/sizeof(BG1_reel1[0]),
    sizeof(BG1_reel2)/sizeof(BG1_reel2[0]),
    sizeof(BG1_reel3)/sizeof(BG1_reel3[0]),
    sizeof(BG1_reel4)/sizeof(BG1_reel4[0]),
    sizeof(BG1_reel5)/sizeof(BG1_reel5[0])
};


///////////////////// general utilities ///////////////////////////////////
void printPayWindow(const vector<vector<Symbol>>& window) {
    cout << "\n===== PAY WINDOW =====\n";
    int rows = window.size();
    int reels = window[0].size();
    for (int row = 0; row < rows; row++) {
        for (int reel = 0; reel < reels; reel++) {

            switch (window[row][reel]) {
                case LV1:  cout << "LV1\t"; break;
                case LV2:  cout << "LV2\t"; break;
                case LV3:  cout << "LV3\t"; break;
                case LV4:  cout << "LV4\t"; break;

                case HV1:  cout << "HV1\t"; break;
                case HV2:  cout << "HV2\t"; break;
                case HV3:  cout << "HV3\t"; break;
                case HV4:  cout << "HV4\t"; break;

                case WILD: cout << "WILD\t"; break;
                case HAT:  cout << "HAT\t"; break;
                case EXPHAT: cout<< "XHAT\t"; break;

                default:   cout << "UNK\t"; break;
            }
        }
        cout << "\n";
    }

    cout << "======================\n";
}

int getRandom(int a,int b){
    std::uniform_int_distribution<int> dist(a, b);
    return dist(rng);
}

double getUniform(){
    std::uniform_real_distribution<double>dist(0.0,1.0);
    return dist(rng);
}

vector<int> SRSWOR(vector<int>allowedPositions,int number){
    vector<int>selectedPositions;
    int totalSize = allowedPositions.size();
    shuffle(
        allowedPositions.begin(),
        allowedPositions.end(),
        rng
    );
    for(int i = 0; i < number && i < allowedPositions.size(); i++){
        selectedPositions.push_back(allowedPositions[i]);
    }
    return selectedPositions;
}

template<typename T>
void print1D(const std::vector<T>& arr)
{
    for (const auto& x : arr)
        std::cout << x << ",";

    std::cout << "\n";
}


vector<Symbol> sliceReels(vector<vector<Symbol>> &pay_window,int reel_idx, int start_idx){
    vector<Symbol>sliced_reel;
    for (int i = start_idx; i < no_of_rows;i++){
        sliced_reel.push_back(pay_window[i][reel_idx]);
    }
    return sliced_reel;

}

struct symbolWhere{
    vector <pair <int,int>> positions;
    Symbol current_symbol;
    vector<int> rawPositions;
    int count = 0;
};

symbolWhere findSymbol(const vector<vector<Symbol>> &pay_window, Symbol target_symbol){
    symbolWhere target_info;
    int rows = pay_window.size();
    int reels = pay_window[0].size();
    target_info.current_symbol = target_symbol;
    for (int i = 0; i < rows; i++){
        for(int j = 0; j < reels; j++){
            if (pay_window[i][j] == target_symbol){
                target_info.positions.push_back({i,j});
                target_info.rawPositions.push_back(i*reels + j);
                target_info.count ++;
            }
        }
    }
    return target_info;
}

int cumProbability(vector<float>probArray,float p){
    vector<float>cumProb(probArray.size(),0);
    float running_sum = 0.0f;
    for(int i = 0; i < probArray.size(); i++){
        running_sum += probArray[i];
        cumProb[i] = running_sum;
    }
    for(int i = 0; i < probArray.size();i++){
        if (p < cumProb[i]) return i;
    }
    return probArray.size() - 1;

}


///////////////////// general utilities ///////////////////////////////////

//////////////////// game specific ////////////////////////////////////

int allHatCount(const vector<vector<Symbol>> &pay_window){
    int count = 0;
    int rows = pay_window.size();
    int reels = pay_window[0].size();
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < reels;j++){
            if((pay_window[i][j] == HAT) || (pay_window[i][j] == EXPHAT) || (pay_window[i][j] == HORHAT) 
            || (pay_window[i][j] == VERTHAT)) count ++;
        }
    }
    return count;
}

struct probTables {
    float girder_threshold = 0.1f;
    vector<float>exp_threshold = {0.75f,0.2f,0.05f};
};

vector<vector<Symbol>> girderTrigger(vector<vector<Symbol>> &pay_window){
    int hat_counts = allHatCount(pay_window);
    vector<int>allowedPositions;
    int rows = pay_window.size();
    int reels = pay_window[0].size();
    for (int i = 0; i < rows; i++){
        for(int j = 0; j < reels; j++){
            if ((pay_window[i][j] != HAT) && ((pay_window[i][j] != EXPHAT)) && (pay_window[i][j] != HORHAT) 
                && (pay_window[i][j] != VERTHAT)){
                    allowedPositions.push_back(i*reels + j);
            }
        }    
    }
    int required_hats = max(0, 6 - hat_counts);
    vector<int>selectedPositions = SRSWOR(allowedPositions,required_hats);
    for(const auto &a: selectedPositions){
        pay_window[a/reels][a%reels] = HAT;
    }
    return pay_window;
}


vector<vector<Symbol>> getActivePayWindow(vector<vector<Symbol>> &pay_window,int active_rows){
    vector<vector<Symbol>> active_pay_window(no_of_rows - active_rows,vector<Symbol>(no_of_reels));
    for(int i = active_rows; i < no_of_rows; i++){
        for(int j = 0; j < no_of_reels; j++){
            active_pay_window[i-active_rows][j] = pay_window[i][j];
        }
    }
    return active_pay_window;
}

struct winElements{
    int round_win = 0;
    int free_game = 0;
};
//////////////////// game specific ////////////////////////////////////



vector<vector<Symbol>> generatePayWindow(Symbol* ReelSet[],vector<int>ReelSize){
    vector<vector<Symbol>> pay_window (no_of_rows,vector<Symbol>(no_of_reels));
    for (int i = 0; i < no_of_reels;i++){
        int start_idx = getRandom(0,ReelSize[i] - 1);
        //cout<<"Index for Reel:"<<start_idx<<endl;
        for (int j = 0; j < no_of_rows;j++){
            int idx = (start_idx + j) % ReelSize[i];
            pay_window[j][i] = ReelSet[i][idx];
        }
    }
    return pay_window;
}


int waysWinCalculation(vector<vector<Symbol>>& pay_window,int start_idx){
    //vector<int>symbolWin{0,no_of_symbols};
    int win = 0;
    vector<Symbol>firstReel = sliceReels(pay_window,0,start_idx);
    //print1D(firstReel);
    vector<int>symbol_count(no_of_symbols,0);
    for (int i = 0; i < firstReel.size();i++){
        symbol_count[firstReel[i]] ++;
    }
    //print1D(symbol_count);
    vector<Symbol>uniqueSymbols;
    for (int i = 0; i < no_of_symbols;i++){
        if (symbol_count[i] > 0) uniqueSymbols.push_back(symbolArray[i]);
    }
    // cout<<"Unique Symbols:"<<endl;
    // print1D(uniqueSymbols);

    for(int s = 0; s < uniqueSymbols.size();s++){
        int left2right = 0;
        int ways = 1;
        for (int idx = 0;idx<no_of_reels;idx++){
            vector<Symbol>sliced_reel = sliceReels(pay_window,idx,start_idx);
            //print1D(sliced_reel);
            int occ_in_reel = 0;
            for(int i = 0; i < sliced_reel.size();i++){
                if ((sliced_reel[i] == uniqueSymbols[s]) || (sliced_reel[i] == WILD))  occ_in_reel ++; 
            }
            if (occ_in_reel > 0){
                ways = ways * occ_in_reel;
                left2right ++;
            }
            else break;
        }
        //cout<<"Ways:"<<ways<<endl;
        int symbol_win = PayTable[uniqueSymbols[s]][left2right] * ways;
        HitTable[uniqueSymbols[s]][left2right] ++;
        WinTable[uniqueSymbols[s]][left2right] = symbol_win;
        // cout<<"Symbol:"<<symbolArray[s]<<",left2right"<<left2right<<",Win:"<<symbol_win<<endl;
        // cout
        //     << "Symbol : " << SymbolChar[uniqueSymbols[s]]
        //     << " | Count : " << left2right
        //     << " | Ways : " << ways
        //     << " | Pay : " << PayTable[uniqueSymbols[s]][left2right]
        //     << " | Total Win : " << symbol_win
        //     << endl;
        win += symbol_win;
    }
    return win;

}

int explosiveHAT(vector<vector<Symbol>> &pay_window,int active_rows){
    symbolWhere EXPHAT_info = findSymbol(pay_window,EXPHAT);
    cout<<"EXPHAT Info:"<<EXPHAT_info.count<<endl;
    probTables prob;
    if (EXPHAT_info.count > 0){
        double p = getUniform();
        int extraRows = active_rows  - (cumProbability(prob.exp_threshold,p) + 1);
        return extraRows;
    }
    return active_rows;
    
}

winElements simulateOneSpin(){
    winElements round_statistics;
    int active_rows = 3;
    int free_game = 0;
    probTables thresh_probs;
    vector<vector<Symbol>>pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
    active_rows = explosiveHAT(pay_window,active_rows);
    int round_win = waysWinCalculation(pay_window,active_rows);
    vector<vector<Symbol>> active_pay_window = getActivePayWindow(pay_window,active_rows);
    int hat_counts = allHatCount(active_pay_window);
    if (hat_counts > 0){
        double p = getUniform();
        if (p < thresh_probs.girder_threshold){
            active_pay_window = girderTrigger(active_pay_window);
        } 
    }
    hat_counts = allHatCount(active_pay_window);
    if (hat_counts >= 6){
        free_game = 1;
    }
    round_statistics.round_win = round_win;
    round_statistics.free_game = free_game;
    return round_statistics;

}

void simulateAll(int N){
    double mean = 0;
    double var = 0;
    int total_free_games = 0;
    for(int i = 0; i < N; i++){
        winElements round_statistics = simulateOneSpin();
        int round_win = round_statistics.round_win;
        total_free_games += round_statistics.free_game;
        double round_win_xbet = round_win * 1.0/bet;
        mean += round_win_xbet;
        var += round_win_xbet*round_win_xbet; 
    }
    mean = mean/N;
    var = var/N - mean*mean;
    double free_game_trigger = 0.0;
    if (total_free_games > 0){
        free_game_trigger = N/total_free_games;
    }
    cout<<"Free Games:"<<free_game_trigger;
    cout<<"RTP:"<<mean<<endl;
    cout<<"Var:"<<var<<endl;
}



int main(){
    simulateAll(10);
    return 0;
}




// void simulateAll(int N){
//     double mean = 0;
//     double var = 0;
//     for (int i = 0; i < N; i++){
//         vector<vector<Symbol>>pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
//         printPayWindow(pay_window);
//         int round_win = waysWinCalculation(pay_window,3);
//         double round_win_xbet = round_win * 1.0/bet;
//         cout<<"Round Win:"<<round_win_xbet<<endl;
//         mean += round_win_xbet;
//         var += round_win_xbet*round_win_xbet; 
//     }
//     mean = mean/N;
//     var = var/N - mean*mean;
//     cout<<"RTP:"<<mean<<endl;
//     cout<<"Var:"<<var<<endl;
// }



// int main(){
    
    
//     //simulateAll(10);
//     // cout<<"Inside Main";
//     cout<<"Just checking win:"<<endl;
//     int active_rows = 3;
//     vector<vector<Symbol>>pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
//     printPayWindow(pay_window);
//     int win =  waysWinCalculation(pay_window,active_rows);
//     vector<vector<Symbol>> active_pay_window = getActivePayWindow(pay_window,active_rows);
//     printPayWindow(active_pay_window);

//     cout<<"Just checking girder trigger:"<<endl;
//     active_pay_window[1][2] = HAT;
//     active_pay_window = girderTrigger(active_pay_window);
//     printPayWindow(active_pay_window);
    
//     cout<<"Just checking explosive hat:"<<endl;
//     pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
//     //pay_window[4][2] = EXPHAT;
//     printPayWindow(pay_window);
//     active_rows = explosiveHAT(pay_window,active_rows);
//     cout<<"active rows:"<<active_rows<<endl;
//     win =  waysWinCalculation(pay_window,active_rows);
//     active_pay_window = getActivePayWindow(pay_window,active_rows);
//     printPayWindow(active_pay_window);
//}








