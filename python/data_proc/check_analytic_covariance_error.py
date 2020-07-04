from time_serial_matrix import *
import math
from tqdm import tqdm
import matplotlib.pyplot as plt
import numpy as np
import scipy.linalg as LA

T, n, d = 100, 1000, 100
tsm = uniform_normal_tsm(T, n, d, loc=80, scale=7.5,
                         random_state=0)

A_F = np.empty(T + 20)
error = np.empty(T + 20)
XTX = np.zeros((d, d))
k = 0
for i in tqdm(range(T + 20)):
    try:
        k2 = t0.index(i + 1, k) 
    except:
        k2 = k
    X = X0[k:k2]
    t = t0[k:k2]
    if len(X) != 0:
        XTX += X.T @ X
    u,s,vh = LA.svd(XTX)
    A_F[i] = np.sum(s)
    error[i] = tsm.analytic_error(vh, np.sqrt(s), i+1)
    k = k2

plt.figure()
plt.title(f'Analytic Covariance Error (n={n}, d={d})')
plt.xlabel('t')
plt.plot(np.arange(T + 20)+1, A_F/d/(np.arange(T + 20)+1),
         label=rf'$\|A\|_F^2/{n**2*math.sqrt(d):.1f}/t^2$')
plt.plot(np.arange(T + 20)+1, error/(n*math.sqrt(d)),
         label=rf'$\|A^T A - B^T B\|_2/{n*math.sqrt(d):.1f}$')
plt.plot(np.arange(T + 20)+1, error/A_F, label=r'$\|A^T A - B^T B\|_2 / \|A\|_F^2$')
plt.legend()
plt.show()

#print(error)
#print(A_F)
print(error / A_F)
