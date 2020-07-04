import numpy as np
from scipy.linalg import svd as svd
from tqdm import tqdm
import matplotlib.pyplot as plt
from time_serial_matrix import *


class ATTP_FD():
    def __init__(self, d, l):
        self.d = d
        self.l = l
        self.AF2 = 0
        self.C = np.zeros((l,d))
        self.Bf = [np.zeros((1, d))]
        self.ft = [0]
        self.fpi = [0]  # 1 index for each full point, index of partial checkpoint
        self.Bp = np.zeros((1, d))
        self.pt = [0]
        self.p = 1
    
    def build(self, X, t):
        XF2 = np.linalg.norm(X, ord='fro')**2
        if XF2 == 0:
            return
        self.AF2 += XF2
        self.C = self._FD(np.concatenate((self.C, X), axis=0), self.l)
        while np.linalg.norm(self.C[0])**2 > self.AF2/self.l/2:
        # while np.linalg.norm(len(self.C)) > 0:
            self._checkpoint(t[0])

    def query(self, t):
        iBf = np.searchsorted(self.ft, t, side='right') - 1
        X = self.Bf[iBf]
        iBp_start = self.fpi[iBf]
        iBp_n = np.searchsorted(self.pt[iBp_start:], t, side='right')
        X = np.concatenate((X, self.Bp[iBp_start:iBp_start+iBp_n]))
        return X

    def query1(self, t):
        iBp = np.searchsorted(self.pt, t, side='right')
        X = self.Bp[:iBp]
        return X

    def _FD(self, X, l):
        _, s, Vt = svd(X, full_matrices=False)
        # TODO: try sklearn.decomposition.TruncatedSVD
        if len(s) > l:
            s2 = s**2  # Sigma^2
            s = np.sqrt(s2[:l] - s2[l-1])
            Vt = Vt[:l]
        return Vt * s.reshape(-1, 1)
    
    def _checkpoint(self, t):
        self.Bp = np.concatenate((self.Bp, self.C[0].reshape(-1, self.d)), axis=0)
        self.C = self.C[1:]
        self.pt.append(t)
        self.p += 1
        if self.p == self.l:
            self.Bf.append(self._FD(np.concatenate(
                (self.Bf[-1], self.Bp[self.fpi[-1]:]), axis=0), self.l))
            self.ft.append(t)
            self.fpi.append(len(self.Bp))
            self.p = 0

    def _checkpoint1(self, t):
        self.Bp = np.concatenate(
            (self.Bp, self.C[0].reshape(-1, self.d)), axis=0)
        self.C = self.C[1:]
        self.pt.append(t)

def error(A, X):
    AF2 = np.linalg.norm(A, ord='fro')**2
    if AF2==0:
        return 0
    ATA_XTX_2 = np.linalg.norm(A.T @ A - X.T @ X, ord=2)
    return ATA_XTX_2/AF2

def _test_small():
    # Data generator
    # T, n, d, loc, scale = 100, 1000, 100, 50, 2
    T, n, d, loc, scale = 1000, 10000, 1000, 500, 20
    tsm = uniform_normal_tsm(T, n, d, loc=loc, scale=scale,
                            random_state=0)
    A = np.empty((0, d))
    At2i = [0]

    # FD_ATTP
    l = 100
    attp_fd = ATTP_FD(d, l)
    for i in tqdm(range(T)):
        X, t = tsm.nextTimeStemps()
        A = np.concatenate((A, X), axis=0)
        At2i.append(At2i[-1]+len(X))
        attp_fd.build(X, t)

    # Query
    # query_times = np.arange(n_steps)*T//(n_steps-1)
    query_times = np.array([10,100,200,400,450,480,500,520,550,600,800,1000])
    n_steps = 11
    errors0 = np.empty(len(query_times))
    errors1 = np.empty(len(query_times))
    errors2 = np.empty(len(query_times))
    for i in tqdm(range(len(query_times))):
        t = query_times[i]
        B = attp_fd.query(t)
        errors1[i] = error(A[:At2i[t]], B)
        B = attp_fd.query1(t)
        errors2[i] = error(A[:At2i[t]], B)
        errors0[i] = error(A[:At2i[t]], np.zeros((l, d)))
    print(errors1)
    plt.figure()
    plt.xlabel('t')
    plt.plot(query_times,
             errors0[:T+1]*100, label=r'$100*\|A^T A\|_2 / \|A\|_F^2$')
    plt.plot(query_times, errors2[:T+1]*100,
             label=r'$100*\|A^T A - B^T B\|_2 / \|A\|_F^2$')
    plt.plot(query_times, errors1[:T+1]*100,
             label=r'$100*\|A^T A - \hat{B}^T \hat{B}\|_2 / \|A\|_F^2$')
    # check points
    u, counts = np.unique(attp_fd.ft, return_counts=True)
    plt.bar(u, counts, width=0.5, label='# Full Check Points')
    u, counts = np.unique(attp_fd.pt, return_counts=True)
    plt.bar(u, counts, width=0.2, label='# Partial Check Points')
    plt.legend()
    plt.show()


if __name__ == '__main__':
    _test_small()
