#include <stdio.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <random>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace std;

const unsigned int DEBUG_SEED = 123456789;
std::mt19937 rng(DEBUG_SEED);
ofstream debug_log;

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


///////////////////// debug utilities ///////////////////////////////////
string symbolToString(Symbol symbol) {
    switch (symbol) {
        case LV1:  return "LV1";
        case LV2:  return "LV2";
        case LV3:  return "LV3";
        case LV4:  return "LV4";
        case HV1:  return "HV1";
        case HV2:  return "HV2";
        case HV3:  return "HV3";
        case HV4:  return "HV4";
        case WILD: return "WILD";
        case HAT:  return "HAT";
        case EXPHAT: return "XHAT";
        case HORHAT: return "HHAT";
        case VERTHAT: return "VHAT";
        case BLANK: return "BLANK";
        default:   return "UNK";
    }
}

void logLine(const string& text = "") {
    debug_log << text << "\n";
}

void logPayWindow(const vector<vector<Symbol>>& window, const string& title) {
    debug_log << "\n===== " << title << " =====\n";
    int rows = window.size();
    int reels = window[0].size();
    debug_log << "rows=" << rows << ", reels=" << reels << "\n";
    debug_log << "        ";
    for (int reel = 0; reel < reels; reel++) {
        debug_log << "R" << reel << "      ";
    }
    debug_log << "\n";

    for (int row = 0; row < rows; row++) {
        debug_log << "row " << row << " | ";
        for (int reel = 0; reel < reels; reel++) {
            debug_log << left << setw(7) << symbolToString(window[row][reel]);
        }
        debug_log << "\n";
    }

    debug_log << "======================\n";
}

void logPositions(const vector<int>& positions, int reels) {
    if (positions.empty()) {
        debug_log << "none";
        return;
    }
    for (size_t i = 0; i < positions.size(); i++) {
        if (i > 0) debug_log << ", ";
        debug_log << positions[i] << "(row=" << positions[i] / reels
                  << ", reel=" << positions[i] % reels << ")";
    }
}

void logInjectedPosition(int row, int reel, Symbol previous_symbol, Symbol injected_symbol) {
    debug_log << "Injected " << symbolToString(injected_symbol)
              << " at row=" << row
              << ", reel=" << reel
              << " replacing " << symbolToString(previous_symbol)
              << "\n";
}

void printPayWindow(const vector<vector<Symbol>>& window) {
    logPayWindow(window, "PAY WINDOW");
}

int getRandom(int a,int b){
    std::uniform_int_distribution<int> dist(a, b);
    int value = dist(rng);
    debug_log << "RNG int [" << a << ", " << b << "] -> " << value << "\n";
    return value;
}

double getUniform(){
    std::uniform_real_distribution<double>dist(0.0,1.0);
    double value = dist(rng);
    debug_log << "RNG uniform real [0.0, 1.0) -> " << value << "\n";
    return value;
}

vector<int> SRSWOR(vector<int>allowedPositions,int number){
    vector<int>selectedPositions;
    int totalSize = allowedPositions.size();
    debug_log << "SRSWOR requested=" << number << ", allowed_count=" << totalSize << "\n";
    debug_log << "SRSWOR allowed before shuffle: ";
    logPositions(allowedPositions, no_of_reels);
    debug_log << "\n";
    shuffle(
        allowedPositions.begin(),
        allowedPositions.end(),
        rng
    );
    debug_log << "SRSWOR allowed after shuffle: ";
    logPositions(allowedPositions, no_of_reels);
    debug_log << "\n";
    for(int i = 0; i < number && i < static_cast<int>(allowedPositions.size()); i++){
        selectedPositions.push_back(allowedPositions[i]);
    }
    debug_log << "SRSWOR selected: ";
    logPositions(selectedPositions, no_of_reels);
    debug_log << "\n";
    return selectedPositions;
}

template<typename T>
void print1D(const std::vector<T>& arr)
{
    for (const auto& x : arr)
        debug_log << x << ",";

    debug_log << "\n";
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
    debug_log << "findSymbol " << symbolToString(target_symbol)
              << ": count=" << target_info.count << ", positions=";
    logPositions(target_info.rawPositions, reels);
    debug_log << "\n";
    return target_info;
}

int cumProbability(vector<float>probArray,float p){
    vector<float>cumProb(probArray.size(),0);
    float running_sum = 0.0f;
    debug_log << "cumProbability p=" << p << ", thresholds=";
    for(size_t i = 0; i < probArray.size(); i++){
        if (i > 0) debug_log << ",";
        debug_log << probArray[i];
        running_sum += probArray[i];
        cumProb[i] = running_sum;
    }
    debug_log << ", cumulative=";
    for (size_t i = 0; i < cumProb.size(); i++) {
        if (i > 0) debug_log << ",";
        debug_log << cumProb[i];
    }
    debug_log << "\n";
    for(size_t i = 0; i < probArray.size();i++){
        if (p < cumProb[i]) {
            debug_log << "cumProbability selected index=" << i << "\n";
            return static_cast<int>(i);
        }
    }
    debug_log << "cumProbability fell through; selected index=" << probArray.size() - 1 << "\n";
    return probArray.size() - 1;

}


///////////////////// debug utilities ///////////////////////////////////

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
    debug_log << "allHatCount rows=" << rows << ", reels=" << reels
              << " -> " << count << "\n";
    return count;
}

struct probTables {
    float girder_threshold = 0.1f;
    vector<float>exp_threshold = {0.75f,0.2f,0.05f};
};

vector<vector<Symbol>> girderTrigger(vector<vector<Symbol>> &pay_window){
    debug_log << "\n-- Girder trigger evaluation --\n";
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
    debug_log << "existing hats=" << hat_counts << ", required_hats=" << required_hats << "\n";
    vector<int>selectedPositions = SRSWOR(allowedPositions,required_hats);
    for(const auto &a: selectedPositions){
        debug_log << "Girder adds HAT at row=" << a / reels << ", reel=" << a % reels << "\n";
        pay_window[a/reels][a%reels] = HAT;
    }
    logPayWindow(pay_window, "ACTIVE PAY WINDOW AFTER GIRDER");
    return pay_window;
}


vector<vector<Symbol>> getActivePayWindow(vector<vector<Symbol>> &pay_window,int active_rows){
    vector<vector<Symbol>> active_pay_window(no_of_rows - active_rows,vector<Symbol>(no_of_reels));
    for(int i = active_rows; i < no_of_rows; i++){
        for(int j = 0; j < no_of_reels; j++){
            active_pay_window[i-active_rows][j] = pay_window[i][j];
        }
    }
    debug_log << "getActivePayWindow active_rows=" << active_rows
              << ", returned_rows=" << no_of_rows - active_rows << "\n";
    return active_pay_window;
}

struct winElements{
    int round_win = 0;
    int free_game = 0;
};
//////////////////// game specific ////////////////////////////////////



vector<vector<Symbol>> generatePayWindow(Symbol* ReelSet[],vector<int>ReelSize){
    vector<vector<Symbol>> pay_window (no_of_rows,vector<Symbol>(no_of_reels));
    debug_log << "\n-- Generating pay window --\n";
    for (int i = 0; i < no_of_reels;i++){
        int start_idx = getRandom(0,ReelSize[i] - 1);
        debug_log << "Reel " << i << ": size=" << ReelSize[i]
                  << ", start_idx=" << start_idx << ", symbols=";
        for (int j = 0; j < no_of_rows;j++){
            int idx = (start_idx + j) % ReelSize[i];
            pay_window[j][i] = ReelSet[i][idx];
            if (j > 0) debug_log << " -> ";
            debug_log << idx << ":" << symbolToString(pay_window[j][i]);
        }
        debug_log << "\n";
    }
    logPayWindow(pay_window, "FULL PAY WINDOW");
    return pay_window;
}


int waysWinCalculation(vector<vector<Symbol>>& pay_window,int start_idx){
    debug_log << "\n-- Ways win calculation --\n";
    debug_log << "start_idx(active rows skipped)=" << start_idx << "\n";
    int win = 0;
    vector<Symbol>firstReel = sliceReels(pay_window,0,start_idx);
    debug_log << "First reel active symbols: ";
    for (size_t i = 0; i < firstReel.size(); i++) {
        if (i > 0) debug_log << ", ";
        debug_log << symbolToString(firstReel[i]);
    }
    debug_log << "\n";
    vector<int>symbol_count(no_of_symbols,0);
    for (size_t i = 0; i < firstReel.size();i++){
        if (firstReel[i] >= 0 && firstReel[i] < no_of_symbols) {
            symbol_count[firstReel[i]] ++;
        } else {
            debug_log << "Ignoring non-paying first-reel symbol for ways seeding: "
                      << symbolToString(firstReel[i])
                      << " (enum=" << firstReel[i] << ")\n";
        }
    }
    debug_log << "First reel symbol counts: ";
    for (int i = 0; i < no_of_symbols; i++) {
        if (i > 0) debug_log << ", ";
        debug_log << SymbolChar[symbolArray[i]] << "=" << symbol_count[i];
    }
    debug_log << "\n";
    vector<Symbol>uniqueSymbols;
    for (int i = 0; i < no_of_symbols;i++){
        if (symbol_count[i] > 0) uniqueSymbols.push_back(symbolArray[i]);
    }
    debug_log << "Unique paying symbols on first active reel: ";
    if (uniqueSymbols.empty()) {
        debug_log << "none";
    }
    for (size_t i = 0; i < uniqueSymbols.size(); i++) {
        if (i > 0) debug_log << ", ";
        debug_log << symbolToString(uniqueSymbols[i]);
    }
    debug_log << "\n";

    for(size_t s = 0; s < uniqueSymbols.size();s++){
        int left2right = 0;
        int ways = 1;
        debug_log << "Evaluating symbol " << symbolToString(uniqueSymbols[s]) << "\n";
        for (int idx = 0;idx<no_of_reels;idx++){
            vector<Symbol>sliced_reel = sliceReels(pay_window,idx,start_idx);
            int occ_in_reel = 0;
            debug_log << "  Reel " << idx << " active slice: ";
            for(size_t i = 0; i < sliced_reel.size();i++){
                if (i > 0) debug_log << ", ";
                debug_log << symbolToString(sliced_reel[i]);
                if ((sliced_reel[i] == uniqueSymbols[s]) || (sliced_reel[i] == WILD))  occ_in_reel ++;
            }
            debug_log << " | matching_or_wild=" << occ_in_reel << "\n";
            if (occ_in_reel > 0){
                ways = ways * occ_in_reel;
                left2right ++;
            }
            else break;
        }
        int pay = PayTable[uniqueSymbols[s]][left2right];
        int symbol_win = pay * ways;
        HitTable[uniqueSymbols[s]][left2right] ++;
        WinTable[uniqueSymbols[s]][left2right] = symbol_win;
        debug_log
            << "  Win result: symbol=" << symbolToString(uniqueSymbols[s])
            << ", left2right=" << left2right
            << ", ways=" << ways
            << ", pay=" << pay
            << ", symbol_win=" << symbol_win
            << "\n";
        win += symbol_win;
    }
    debug_log << "Total ways win=" << win << "\n";
    return win;

}

int explosiveHAT(vector<vector<Symbol>> &pay_window,int active_rows){
    debug_log << "\n-- Explosive HAT evaluation --\n";
    symbolWhere EXPHAT_info = findSymbol(pay_window,EXPHAT);
    debug_log<<"EXPHAT Info:"<<EXPHAT_info.count<<"\n";
    probTables prob;
    if (EXPHAT_info.count > 0){
        double p = getUniform();
        int extraRows = active_rows  - (cumProbability(prob.exp_threshold,p) + 1);
        debug_log << "Explosive HAT active_rows " << active_rows
                  << " -> " << extraRows << "\n";
        return extraRows;
    }
    debug_log << "No EXPHAT present; active_rows remains " << active_rows << "\n";
    return active_rows;

}

winElements simulateOneSpin(int spin_index){
    debug_log << "\n============================================================\n";
    debug_log << "SPIN " << spin_index << "\n";
    debug_log << "============================================================\n";
    winElements round_statistics;
    int active_rows = 3;
    int free_game = 0;
    probTables thresh_probs;
    debug_log << "Initial active_rows=" << active_rows << "\n";
    vector<vector<Symbol>>pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
    active_rows = explosiveHAT(pay_window,active_rows);
    int round_win = waysWinCalculation(pay_window,active_rows);
    vector<vector<Symbol>> active_pay_window = getActivePayWindow(pay_window,active_rows);
    logPayWindow(active_pay_window, "ACTIVE PAY WINDOW BEFORE GIRDER");
    int hat_counts = allHatCount(active_pay_window);
    debug_log << "Bonus pre-check hat_counts=" << hat_counts << "\n";
    if (hat_counts > 0){
        double p = getUniform();
        debug_log << "Girder probability p=" << p
                  << ", threshold=" << thresh_probs.girder_threshold << "\n";
        if (p < thresh_probs.girder_threshold){
            debug_log << "Girder trigger: YES\n";
            active_pay_window = girderTrigger(active_pay_window);
        } else {
            debug_log << "Girder trigger: NO\n";
        }
    } else {
        debug_log << "Girder trigger skipped because no hats are active\n";
    }
    hat_counts = allHatCount(active_pay_window);
    if (hat_counts >= 6){
        free_game = 1;
    }
    debug_log << "Bonus final hat_counts=" << hat_counts
              << ", free_game=" << free_game << "\n";
    debug_log << "Spin summary: round_win=" << round_win
              << ", round_win_xbet=" << (round_win * 1.0 / bet)
              << ", free_game=" << free_game << "\n";
    round_statistics.round_win = round_win;
    round_statistics.free_game = free_game;
    return round_statistics;

}

void runForcedGirderTest() {
    debug_log << "\n############################################################\n";
    debug_log << "FEATURE TEST: FORCED GIRDER VIA INJECTED HAT\n";
    debug_log << "############################################################\n";

    int active_rows = 3;
    vector<vector<Symbol>> pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
    logPayWindow(pay_window, "GIRDER TEST ORIGINAL FULL PAY WINDOW");

    int round_win_before_feature = waysWinCalculation(pay_window, active_rows);
    debug_log << "GIRDER TEST payout before feature: round_win="
              << round_win_before_feature
              << ", round_win_xbet=" << (round_win_before_feature * 1.0 / bet)
              << "\n";

    vector<vector<Symbol>> active_pay_window = getActivePayWindow(pay_window, active_rows);
    logPayWindow(active_pay_window, "GIRDER TEST ORIGINAL ACTIVE PAY WINDOW");

    vector<pair<int,int>> injection_positions = {{0, 0}, {1, 2}};
    debug_log << "GIRDER TEST injected symbol positions:\n";
    for (const auto& pos : injection_positions) {
        int row = pos.first;
        int reel = pos.second;
        Symbol previous_symbol = active_pay_window[row][reel];
        active_pay_window[row][reel] = HAT;
        logInjectedPosition(row, reel, previous_symbol, HAT);
    }

    logPayWindow(active_pay_window, "GIRDER TEST MODIFIED ACTIVE PAY WINDOW");
    int hat_counts_before = allHatCount(active_pay_window);
    debug_log << "GIRDER TEST forced trigger: calling girderTrigger directly\n";
    active_pay_window = girderTrigger(active_pay_window);
    int hat_counts_after = allHatCount(active_pay_window);
    int free_game = hat_counts_after >= 6 ? 1 : 0;
    debug_log << "GIRDER TEST state change: hats_before=" << hat_counts_before
              << ", hats_after=" << hat_counts_after
              << ", free_game=" << free_game << "\n";
    debug_log << "GIRDER TEST payout after feature: round_win remains "
              << round_win_before_feature
              << " because core ways payout is calculated before girder logic\n";
}

void runForcedExplosiveHatTest() {
    debug_log << "\n############################################################\n";
    debug_log << "FEATURE TEST: FORCED EXPHAT/HAT ACTIVE ROW EXPANSION\n";
    debug_log << "############################################################\n";

    int active_rows = 3;
    vector<vector<Symbol>> pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
    logPayWindow(pay_window, "EXPHAT TEST ORIGINAL FULL PAY WINDOW");

    int inject_row = 4;
    int inject_reel = 0;
    Symbol previous_symbol = pay_window[inject_row][inject_reel];
    pay_window[inject_row][inject_reel] = EXPHAT;
    debug_log << "EXPHAT TEST injected symbol positions:\n";
    logInjectedPosition(inject_row, inject_reel, previous_symbol, EXPHAT);
    logPayWindow(pay_window, "EXPHAT TEST MODIFIED FULL PAY WINDOW");

    double forced_probability = 0.10;
    probTables prob;
    debug_log << "EXPHAT TEST forced trigger: EXPHAT present, forced p="
              << forced_probability << "\n";
    int selected_index = cumProbability(prob.exp_threshold, forced_probability);
    int expanded_active_rows = active_rows - (selected_index + 1);
    debug_log << "EXPHAT TEST active_rows " << active_rows
              << " -> " << expanded_active_rows
              << " using exp_threshold index=" << selected_index << "\n";

    int round_win_before = waysWinCalculation(pay_window, active_rows);
    vector<vector<Symbol>> active_before = getActivePayWindow(pay_window, active_rows);
    logPayWindow(active_before, "EXPHAT TEST ACTIVE WINDOW BEFORE EXPANSION");
    int hats_before = allHatCount(active_before);

    int round_win_after = waysWinCalculation(pay_window, expanded_active_rows);
    vector<vector<Symbol>> active_after = getActivePayWindow(pay_window, expanded_active_rows);
    logPayWindow(active_after, "EXPHAT TEST ACTIVE WINDOW AFTER EXPANSION");
    int hats_after = allHatCount(active_after);
    int free_game = hats_after >= 6 ? 1 : 0;

    debug_log << "EXPHAT TEST payout/state change: round_win_before="
              << round_win_before
              << ", round_win_after=" << round_win_after
              << ", hats_before=" << hats_before
              << ", hats_after=" << hats_after
              << ", free_game=" << free_game << "\n";
}

void runHatWaysSeedTest() {
    debug_log << "\n############################################################\n";
    debug_log << "FEATURE TEST: INJECTED HAT ON FIRST ACTIVE REEL\n";
    debug_log << "############################################################\n";

    int active_rows = 3;
    vector<vector<Symbol>> pay_window = generatePayWindow(BG1_Reels,BG1_Reelsize);
    logPayWindow(pay_window, "HAT TEST ORIGINAL FULL PAY WINDOW");

    int inject_row = active_rows;
    int inject_reel = 0;
    Symbol previous_symbol = pay_window[inject_row][inject_reel];
    pay_window[inject_row][inject_reel] = HAT;
    debug_log << "HAT TEST injected symbol positions:\n";
    logInjectedPosition(inject_row, inject_reel, previous_symbol, HAT);
    logPayWindow(pay_window, "HAT TEST MODIFIED FULL PAY WINDOW");

    debug_log << "HAT TEST triggered feature: ways calculation with HAT on first active reel\n";
    int round_win = waysWinCalculation(pay_window, active_rows);
    vector<vector<Symbol>> active_pay_window = getActivePayWindow(pay_window, active_rows);
    logPayWindow(active_pay_window, "HAT TEST ACTIVE PAY WINDOW");
    int hat_counts = allHatCount(active_pay_window);
    debug_log << "HAT TEST state: round_win=" << round_win
              << ", hats_active=" << hat_counts
              << ", note=HAT is counted for bonus state but not seeded as a paying ways symbol\n";
}

void runFeatureSpecificTests() {
    debug_log << "\n\n============================================================\n";
    debug_log << "FEATURE-SPECIFIC FORCED TEST CASES\n";
    debug_log << "============================================================\n";
    runForcedGirderTest();
    runForcedExplosiveHatTest();
    runHatWaysSeedTest();
}

void logTables() {
    debug_log << "\n===== HIT TABLE =====\n";
    for (int symbol = 0; symbol < no_of_symbols; symbol++) {
        debug_log << SymbolChar[symbolArray[symbol]] << ": ";
        for (int match = 0; match < MAX_MATCH; match++) {
            if (match > 0) debug_log << ", ";
            debug_log << match << "x=" << HitTable[symbol][match];
        }
        debug_log << "\n";
    }
    debug_log << "\n===== WIN TABLE =====\n";
    for (int symbol = 0; symbol < no_of_symbols; symbol++) {
        debug_log << SymbolChar[symbolArray[symbol]] << ": ";
        for (int match = 0; match < MAX_MATCH; match++) {
            if (match > 0) debug_log << ", ";
            debug_log << match << "x=" << WinTable[symbol][match];
        }
        debug_log << "\n";
    }
}

void simulateAll(int N){
    debug_log << "Huff n Puff High Rise core_test debug run\n";
    debug_log << "DEBUG_SEED=" << DEBUG_SEED << "\n";
    debug_log << "Spins=" << N << ", bet=" << bet << "\n";
    debug_log << "Note: getUniform() preserves core.cpp behavior by returning a sampled double.\n";
    double mean = 0;
    double var = 0;
    int total_free_games = 0;
    for(int i = 0; i < N; i++){
        winElements round_statistics = simulateOneSpin(i + 1);
        int round_win = round_statistics.round_win;
        total_free_games += round_statistics.free_game;
        double round_win_xbet = round_win * 1.0/bet;
        mean += round_win_xbet;
        var += round_win_xbet*round_win_xbet;
    }
    mean = mean/N;
    var = var/N - mean*mean;
    debug_log << "\n===== SIMULATION SUMMARY =====\n";
    debug_log << "total_free_games=" << total_free_games << "\n";
    double free_game_trigger = 0.0;
    if (total_free_games > 0) {
        free_game_trigger = N/total_free_games;
    }
    debug_log << "Free Games:" << free_game_trigger << "\n";
    debug_log << "RTP:" << mean << "\n";
    debug_log << "Var:" << var << "\n";
    logTables();
}



int main(){
    std::filesystem::create_directories("outputs");
    debug_log.open("outputs/test.txt", ios::out | ios::trunc);
    if (!debug_log.is_open()) {
        cerr << "Failed to open outputs/test.txt\n";
        return 1;
    }
    simulateAll(25);
    runFeatureSpecificTests();
    debug_log.close();
    return 0;
}
