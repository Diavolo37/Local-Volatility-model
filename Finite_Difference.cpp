#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <string>

using namespace std;

struct Node {
    double K;
    double T;
    double C;
    double localVol;
};

int main() {
    string inputFile = "smoothed_call_grid.csv";
    string outputFile = "local_vol_surface.csv";
    
    const int numStrikes = 100;
    const int numTimes = 100;
    const double riskFreeRate = 0.05;

    // Allocate 2D structure
    vector<vector<Node>> grid(numStrikes, vector<Node>(numTimes));

    // Parse Input CSV
    ifstream file(inputFile);
    if (!file.is_open()) {
        cerr << "Error: Missing input dataset " << inputFile << endl;
        return 1;
    }

    string line, token;
    getline(file, line); // Skip header row

    int i = 0, j = 0;
    while (getline(file, line)) {
        stringstream ss(line);
        Node n;
        n.localVol = 0.0;

        getline(ss, token, ','); n.K = stod(token);
        getline(ss, token, ','); n.T = stod(token);
        getline(ss, token, ','); n.C = stod(token);

        grid[i][j] = n;
        
        j++;
        if (j == numTimes) {
            j = 0;
            i++;
        }
    }
    file.close();

    // Finite Difference Engine Loop (FTCS)
    for (int s = 1; s < numStrikes - 1; s++) {
        for (int t = 1; t < numTimes - 1; t++) {
            
            double dK = grid[s+1][t].K - grid[s][t].K;
            double dT = grid[s][t+1].T - grid[s][t].T;
            
            double K = grid[s][t].K;
            double C = grid[s][t].C;

            // Forward Difference for Time (Theta)
            double dC_dT = (grid[s][t+1].C - C) / dT;

            // Central Difference for Strike (Delta)
            double dC_dK = (grid[s+1][t].C - grid[s-1][t].C) / (2.0 * dK);

            // Second-Order Central Difference for Strike Curvature (Gamma)
            double d2C_dK2 = (grid[s+1][t].C - 2.0 * C + grid[s-1][t].C) / (dK * dK);

            // Synthesizing using Dupire's Equation
            double denominator = 0.5 * K * K * d2C_dK2;
            double numerator = dC_dT + (riskFreeRate * K * dC_dK);

            if (denominator <= 1e-8 || numerator < 0 || std::isnan(numerator) || std::isnan(denominator)) {
                grid[s][t].localVol = 0.0; 
            } else {
                grid[s][t].localVol = sqrt(numerator / denominator);
            }
        }
    }

    // Export unbroken grid to target CSV
    ofstream outFile(outputFile);
    outFile << "Strike,Time,LocalVolatility\n";
    
    // Iterate across calculated nodes
    for (int s = 1; s < numStrikes - 1; s++) {
        for (int t = 1; t < numTimes - 1; t++) {
            double vol = grid[s][t].localVol;
            
            // Institutional Volatility Floor: Patches arbitrage potholes to keep grid rectangular
            if (vol <= 0.001 || std::isnan(vol)) {
                vol = 0.01; // 1% Baseline floor avoids silent out-of-bounds NaN corruption
            }

            outFile << grid[s][t].K << "," << grid[s][t].T << "," << vol << "\n";
        }
    }
    outFile.close();
    
    cout << "Success: Generated dense 'local_vol_surface.csv'." << endl;
    return 0;
}
