#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

using namespace std;

// --- 1. BLACK-SCHOLES ANALYTICAL MATH ---
double norm_cdf(double x) {
    return 0.5 * std::erfc(-x / sqrt(2.0));
}

double bs_call_price(double S, double K, double T, double r, double sigma) {
    if (T <= 0.0) return max(S - K, 0.0);
    double d1 = (log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrt(T));
    double d2 = d1 - sigma * sqrt(T);
    return S * norm_cdf(d1) - K * exp(-r * T) * norm_cdf(d2);
}

// --- 2. LOCAL VOLATILITY SURFACE LOADER ---
class LocalVolSurface {
private:
    vector<double> strikes;
    vector<double> times;
    vector<vector<double>> volMatrix;

public:
    LocalVolSurface(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Fatal Error: Missing surface map " << filename << endl;
            exit(1);
        }

        string line, token;
        getline(file, line); 

        double lastT = -1.0;
        vector<double> currentRow;

        while (getline(file, line)) {
            stringstream ss(line);
            double K, T, vol;
            
            getline(ss, token, ','); K = stod(token);
            getline(ss, token, ','); T = stod(token);
            getline(ss, token, ','); vol = stod(token);

            if (lastT == -1.0) lastT = T;

            if (T != lastT) {
                times.push_back(lastT);
                volMatrix.push_back(currentRow);
                currentRow.clear();
                lastT = T;
            }

            if (volMatrix.empty()) strikes.push_back(K);
            
            currentRow.push_back(vol);
        }
        times.push_back(lastT);
        volMatrix.push_back(currentRow);
        file.close();
        
        cout << "System: Local Volatility surface loaded safely into memory." << endl;
    }

    double getVol(double S, double t) {
        if (S <= strikes.front()) S = strikes.front();
        if (S >= strikes.back()) S = strikes.back();
        if (t <= times.front()) t = times.front();
        if (t >= times.back()) t = times.back();

        auto k_it = lower_bound(strikes.begin(), strikes.end(), S);
        int k_idx = max(0, (int)distance(strikes.begin(), k_it) - 1);

        auto t_it = lower_bound(times.begin(), times.end(), t);
        int t_idx = max(0, (int)distance(times.begin(), t_it) - 1);

        return volMatrix[t_idx][k_idx];
    }
};

int main() {
    // --- Configuration Parameters ---
    // Note: S0 should ideally be calibrated to your exact Python stock output
    double S0 = 105.0;        
    double K_target = 65.0;  
    double T_target = 0.5;    
    double r = 0.05;          
    
    int numPaths = 10000;     
    int numSteps = 100;       
    double dt = T_target / numSteps;

    LocalVolSurface surface("local_vol_surface.csv");

    mt19937 generator(42); 
    normal_distribution<double> standardNormal(0.0, 1.0);

    double sumPayoffs = 0.0;
    cout << "System: Launching stochastic valuation engine..." << endl;

    // --- LOCAL VOLATILITY MONTE CARLO ---
    for (int i = 0; i < numPaths; i++) {
        double St = S0;
        
        for (int step = 0; step < numSteps; step++) {
            double t = step * dt;
            double sigma = surface.getVol(St, t);
            double Z = standardNormal(generator);
            St = St * exp((r - 0.5 * sigma * sigma) * dt + sigma * sqrt(dt) * Z);
        }

        double payoff = max(St - K_target, 0.0);
        sumPayoffs += payoff;
    }

    double simulatedPrice = exp(-r * T_target) * (sumPayoffs / numPaths);

    // --- NAIVE BLACK-SCHOLES COMPARISON ---
    double constant_vol = surface.getVol(S0, 0.0); 
    double bs_price = bs_call_price(S0, K_target, T_target, r, constant_vol);

    cout << "========================================" << endl;
    cout << " Target Option Strike : $" << K_target << endl;
    cout << " Baseline Volatility  :  " << (constant_vol * 100.0) << "%" << endl;
    cout << "----------------------------------------" << endl;
    cout << " Naive BS Price       : $" << bs_price << endl;
    cout << " Local Vol MC Price   : $" << simulatedPrice << endl;
    
    double premium = simulatedPrice - bs_price;
    cout << " Skew Premium         : $" << premium << " (The cost of fear)" << endl;
    cout << "========================================" << endl;
    
    return 0;
}
