#!/usr/bin/python3

import numpy as np
#import matplotlib.pyplot as plt
from scipy import linalg as LA
from scipy.sparse.linalg import eigs
#from codecs import *
import math
import copy
import functools

class FD:
    def __init__(self, l, d):
        self.B = np.zeros((2 * l, d))
        self.l = l
        self.zeros = list(range(len(self.B) - 1, -1, -1))

    def svd(self):
        _, S, V = LA.svd(self.B, full_matrices=False)
        epsilon = pow(S[self.l-1:self.l], 2)
        Sp = np.zeros(self.B.shape[0])
        for j in range(self.l - 1):
            Sp[j] = math.sqrt(pow(S[j:j+1], 2) - epsilon)
        Sp = np.diag(Sp)
        self.B = np.matmul(Sp, V)
        self.zeros = []
        for k in range(len(self.B) - 1, -1, -1):
            if np.linalg.norm(self.B[k:k+1,:], 2) == 0:
                self.zeros.append(k)

    def update(self, row):
        if len(self.zeros) == 0:
            self.svd()
        k = self.zeros.pop()
        self.B[k:k+1, :] = row


    def update_mat(self, mat):
        for row in mat:
            self.update(row)

    def pop_first(self):
        ret = np.zeros(self.B.shape[1])
        for i in range(len(self.B)):
            if np.linalg.norm(self.B[i:i+1], 2) > 0:
                ret = copy.deepcopy(self.B[i:i+1, :])
                self.B[i:i+1, :] = np.zeros(ret.shape)
                self.zeros.append(i)
                break
        return ret

    def to_mat(self):
        return self.B

class FDATTP:
    def __init__(self, l, d):
        self.AF2 = .0
        self.l = l
        self.d = d
        self.fd = FD(l, d)
        self.ckpt = [[0, [], FD(l, d)],
                     [float("inf"), [], None]] #entry = [ts, list_of_partial_checkpoint, full_checkpoint]

    def update(self, ts, row):
        self.fd.update(row)
        self.AF2 += np.linalg.norm(row, 2) ** 2
        E, _ = eigs(np.matmul(self.fd.to_mat().T, self.fd.to_mat()), k = 1)
        if E[0] >= self.AF2/self.l:
            r = self.fd.pop_first()
            self.ckpt[-1][1].append((ts, r)) #partial_ckpt.append
            if len(self.ckpt[-1][1]) >= self.l:
                new_full_ckpt = copy.deepcopy(self.ckpt[-2][2])
                for r in self.ckpt[-1][1]:
                    new_full_ckpt.update(r[1])
                self.ckpt[-1][0] = ts
                self.ckpt[-1][2] = new_full_ckpt
                self.ckpt.append([float("inf"), [], None])

    def query(self, ts):
        ret = FD(self.l, self.d)
        for pair in self.ckpt:
            for partial in pair[1]:
                if partial[0] <= ts:
                    ret.update(partial[1])
                else:
                    return ret.to_mat()
        return ret.to_mat()
        for (i, pair) in enumerate(self.ckpt):
            if pair[0] <= ts and self.ckpt[i + 1][0] > ts:
                ret = copy.deepcopy(pair[2])
                for pair in self.ckpt[i+1][1]:
                    if pair[0] <= ts:
                        ret.update(pair[1])
                    else:
                        break
                return ret.to_mat()

    def mem_usage(self):
        ret = 0.0
        for i in self.ckpt:
            if len(i[1]) > 0:
                ret += len(i[1]) * len(i[1][0]) * 4
            ret += self.l * 2 * self.d * 4
        return ret

fname = 'data/matrix_small.txt'
TS = [] #timestamp
A = [] #data
Q = [] #queries
for line in open(fname, 'r', encoding='utf-8'): 
    if line[0] == '?':
        Q.append(int(line[1:]))
    else:
        entries = list(map(float, line.split()))
        TS.append(int(entries[0]))
        A.append(entries[1:])

A = np.array(A)

BL = [] #baseline
t = 0
for l in [20]:
    f = FDATTP(l, A.shape[1])
    for (i, (ts, row)) in enumerate(zip(TS, A)):
        f.update(ts, row)
        if t < len(Q) and ts > Q[t]:
            BL.append(i)
            t += 1
    while t < len(Q):
        BL.append(len(A))
        t += 1
    
    print("l=%d, mem=%fMB" % (l, f.mem_usage() / 1024.0 / 1024.0))

    
    print("[debug] f: num_full_ckpt = %d, num_partial_ckpt = %d" % (
            len(f.ckpt) -  1,
            functools.reduce(lambda acc, ckpt: acc + len(ckpt[1]), f.ckpt, 0)
        ))
    for i in range(len(Q)):
        B = f.query(Q[i])
        BS = A[:BL[i]]
        norm = LA.norm(BS, 'fro') 
        err = (LA.norm(np.matmul(BS.T, BS) - np.matmul(B.T,B))) / (norm ** 2)
        print("Query %d,  l = %d, norm = %f, err = %f" % (Q[i], l, norm, err))


"""
    f = FD(l, A.shape[1])
    for (i, row) in enumerate(A):
        f.update(row)
    B = f.to_mat()
    errmax = LA.norm(np.matmul(A.T,A) - np.matmul(B.T,B))
    print("FD: When l =", l, ",errmax:",errmax)
"""



