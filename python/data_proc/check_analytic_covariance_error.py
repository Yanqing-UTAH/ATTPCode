from time_serial_matrix import *
import math
from tqdm import tqdm
import matplotlib.pyplot as plt
import numpy as np
import scipy.linalg as LA

T, n, d = 10, 1000, 100
tsm = uniform_normal_tsm(T, n, d, loc=80, scale=7.5,
                         random_state=1)

A_F = np.empty(T)
error = np.empty(T)
XTX = np.zeros((d, d))
for i in tqdm(range(T)):
    X, t = tsm.nextTimeStemps()
    XTX += X.T @ X
    u,s,vh = LA.svd(XTX)
    A_F[i] = np.sum(s**2)
    error[i] = tsm.analytic_error(vh, np.sqrt(s), i+1)

plt.figure()
plt.title(f'Analytic Covariance Error (n={n}, d={d})')
plt.xlabel('t')
plt.plot(np.arange(T)+1, A_F/d/(np.arange(T)+1),
         label=rf'$\|A\|_F^2/{n**2*math.sqrt(d):.1f}/t^2$')
plt.plot(np.arange(T)+1, error/(n*math.sqrt(d)),
         label=rf'$\|A^T A - B^T B\|_2/{n*math.sqrt(d):.1f}$')
plt.plot(np.arange(T)+1, error/A_F, label=r'$\|A^T A - B^T B\|_2 / \|A\|_F^2$')
plt.legend()
plt.show()
