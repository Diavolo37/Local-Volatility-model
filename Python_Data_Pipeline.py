import yfinance as yf
import numpy as np
import pandas as pd
import scipy.stats as si
from scipy.interpolate import griddata
import matplotlib.pyplot as plt
from matplotlib import cm
from datetime import datetime
import warnings

# Suppress warnings for cleaner console output
warnings.filterwarnings('ignore')

# ==========================================
# 1. THE MATH ENGINE (Newton-Raphson)
# ==========================================
def black_scholes_call(S, K, T, r, sigma):
    """Calculates the theoretical Black-Scholes price for a Call Option."""
    if T <= 0 or sigma <= 0: return max(0.0, S - K)
    d1 = (np.log(S / K) + (r + 0.5 * sigma ** 2) * T) / (sigma * np.sqrt(T))
    d2 = d1 - sigma * np.sqrt(T)
    return S * si.norm.cdf(d1) - K * np.exp(-r * T) * si.norm.cdf(d2)

def bs_vega(S, K, T, r, sigma):
    """Calculates the Vega of a Call Option."""
    if T <= 0 or sigma <= 0: return 0.0
    d1 = (np.log(S / K) + (r + 0.5 * sigma ** 2) * T) / (sigma * np.sqrt(T))
    return S * np.sqrt(T) * si.norm.pdf(d1)

def implied_volatility_newton(market_price, S, K, T, r, tol=1e-5, max_iter=100):
    """Extracts IV from a market price using the Newton-Raphson method."""
    intrinsic_val = max(0.0, S - K * np.exp(-r * T))
    if market_price <= intrinsic_val:
        return np.nan

    sigma = 0.20 # Initial guess
    for _ in range(max_iter):
        price = black_scholes_call(S, K, T, r, sigma)
        diff = price - market_price
        if abs(diff) < tol:
            return sigma
        vega = bs_vega(S, K, T, r, sigma)
        if abs(vega) < 1e-4: break
        sigma = sigma - diff / vega
        if sigma <= 0 or sigma > 5.0: break
    return np.nan 

# ==========================================
# 2. THE DATA INGESTION PIPELINE
# ==========================================
def fetch_and_calculate_iv(ticker_symbol="SPY", risk_free_rate=0.05, max_expirations=4):
    print(f"Fetching data for {ticker_symbol}...")
    ticker = yf.Ticker(ticker_symbol)
    S = ticker.history(period="1d")['Close'].iloc[-1]
    expirations = ticker.options

    if not expirations:
        print("Error: No options data found.")
        return pd.DataFrame(), S

    print(f"Current Underlying Price (S): ${S:.2f}")
    iv_data = []
    today = datetime.today()

    for exp_date_str in expirations[:max_expirations]:
        exp_date = datetime.strptime(exp_date_str, "%Y-%m-%d")
        days_to_expiry = (exp_date - today).days
        if days_to_expiry <= 0: continue
        T = days_to_expiry / 365.0

        chain = ticker.option_chain(exp_date_str)
        calls = chain.calls
        calls = calls[calls['lastPrice'] > 0.0]

        for index, row in calls.iterrows():
            K = row['strike']
            market_price = row['lastPrice']
            iv = implied_volatility_newton(market_price, S, K, T, risk_free_rate)
            if not np.isnan(iv):
                iv_data.append({
                    'Strike (K)': K,
                    'Time to Expiry (T)': round(T, 4),
                    'Market Price': market_price,
                    'Implied Volatility': round(iv, 4)
                })

    df = pd.DataFrame(iv_data)
    print(f"Successfully processed {len(df)} valid call options.")
    return df, S

# ==========================================
# 3. INTERPOLATION & EXPORT
# ==========================================
def plot_and_export_surface(df, S, risk_free_rate=0.05):
    if df.empty:
        print("DataFrame is empty. Cannot process surface.")
        return

    X_raw = df['Strike (K)'].values
    Y_raw = df['Time to Expiry (T)'].values
    Z_raw = df['Implied Volatility'].values

    X_grid = np.linspace(X_raw.min(), X_raw.max(), 100)
    Y_grid = np.linspace(Y_raw.min(), Y_raw.max(), 100)
    X_mesh, Y_mesh = np.meshgrid(X_grid, Y_grid)

    Z_mesh = griddata(
        points=(X_raw, Y_raw),
        values=Z_raw,
        xi=(X_mesh, Y_mesh),
        method='cubic'
    )

    # RESTORED: Plotting the 3D Surface
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111, projection='3d')

    surf = ax.plot_surface(X_mesh, Y_mesh, Z_mesh, cmap=cm.viridis,
                           edgecolor='none', alpha=0.8)
    ax.scatter(X_raw, Y_raw, Z_raw, color='black', s=15, label='Raw Market Data')

    ax.set_title('Implied Volatility Surface (Smoothed)')
    ax.set_xlabel('Strike Price (K)')
    ax.set_ylabel('Time to Expiry (Years)')
    ax.set_zlabel('Implied Volatility (IV)')
    fig.colorbar(surf, shrink=0.5, aspect=5)
    plt.legend()
    plt.show()

    # Convert smoothed IV mesh back into perfectly smooth Call Prices using Vectorization
    vectorized_bs = np.vectorize(black_scholes_call)
    C_mesh = vectorized_bs(S, X_mesh, Y_mesh, risk_free_rate, Z_mesh)
    
    smoothed_data = pd.DataFrame({
        'Strike': X_mesh.flatten(),
        'Time': Y_mesh.flatten(),
        'CallPrice': C_mesh.flatten()
    })

    smoothed_data = smoothed_data.dropna()
    smoothed_data.to_csv("smoothed_call_grid.csv", index=False)
    print(f"Saved clean grid (using S = ${S:.2f}) to 'smoothed_call_grid.csv' for C++ engine.")

if __name__ == "__main__":
    iv_surface_df, current_price = fetch_and_calculate_iv("SPY", risk_free_rate=0.05, max_expirations=4)
    plot_and_export_surface(iv_surface_df, current_price)
