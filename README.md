Local Volatility Options Pricing Engine

A production-grade quantitative finance pipeline built in C++ and Python. This project translates real-world market implied volatility (the "Volatility Smile") into a deterministic Local Volatility surface via Dupire's Equation, allowing for arbitrage-free pricing of path-dependent exotic options via Monte Carlo simulation.

Core Architecture

Python Data Ingestion & Interpolation (pipeline.py): Connects to Yahoo Finance to scrape live options chains. Utilizes a Newton-Raphson root-finder to extract implied volatilities, applies 2D spline interpolation to construct a continuous arbitrage-free surface, and back-calculates a dense grid of synthetic Call prices.

C++ Dupire Finite Difference Engine (dupire_engine.cpp): Ingests the synthetic Call price grid. Implements a Forward-Time Central-Space (FTCS) finite difference scheme to calculate the partial derivatives of Dupire's equation, mapping average implied volatilities into instantaneous local volatilities mapped to physical asset price coordinates.

C++ Monte Carlo Validator (mc_validator.cpp): A high-speed stochastic physics engine. Executes 10,000+ simulated random walks using Euler-Maruyama discretization. Paths dynamically query the local volatility coordinate map at every micro-step to replicate the exact market "skew premium" over standard Black-Scholes pricing.

How to Run

1. Data Generation (Python)

Extract real-world data and build the continuous price surface.

pip install yfinance numpy pandas scipy matplotlib
python pipeline.py


(Outputs smoothed_call_grid.csv)

2. Local Volatility Engine (C++)

Translate the market prices into an instantaneous volatility map.

g++ -O3 dupire_engine.cpp -o dupire_engine
./dupire_engine


(Outputs local_vol_surface.csv)

3. Validation & Pricing (C++)

Run the stochastic simulation to price an option based on the localized volatility.

g++ -O3 mc_validator.cpp -o validator
./validator
