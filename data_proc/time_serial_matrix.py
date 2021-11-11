import numpy as np
from scipy.stats import beta, uniform, norm, skewnorm
from sklearn.utils import check_array, check_random_state
from scipy.stats import ortho_group, norm
from scipy.fftpack import dct
import scipy.linalg as LA
import math


class TimeSerialMatrix():
    def __init__(self, dim=100, random_state=None):
        self.dim = dim
        self.ts_list = []
        self.var_list = []
        self.sd_list = []
        self.dir_list = []
        self.type_list = np.empty(0, dtype=int)
        self.current_index = 0
        self.current_ts = 0
        self.random = check_random_state(random_state)

    def add_type(self, vars, dirs, ts_list):
        self.type_list = np.concatenate(
            (self.type_list, np.ones(len(ts_list), dtype=int) * len(self.var_list)))
        self.var_list.append(vars)
        self.sd_list.append(np.sqrt(vars))
        self.dir_list.append(dirs)
        self.ts_list = np.concatenate((self.ts_list, ts_list))
        order = np.argsort(self.ts_list)
        self.ts_list = self.ts_list[order]
        self.type_list = self.type_list[order]

    def nextVectors(self, n=1):
        if self.current_index+n > len(self.ts_list):
            n = len(self.ts_list) - self.current_index
        if n==0:
            return np.empty((0, self.dim)), np.empty(0)
        t = self.ts_list[self.current_index:self.current_index+n]
        X = np.empty((n, self.dim), dtype=np.half)
        u, indices, counts = np.unique(
            self.type_list[self.current_index:self.current_index+n], return_inverse=True, return_counts=True)
        for i in range(len(u)):
            if counts[i] == 0:
                continue
            x = self.random.normal(scale=self.sd_list[u[i]], size=(
                counts[i], len(self.sd_list[u[i]]))).astype('float16')
            if self.dir_list[u[i]] is not None:
                X[indices == i] = x @ self.dir_list[u[i]]
            else:
                X[indices == i] = dct(x, 4, norm='ortho')
        self.current_index += n
        return X, t

    def nextTimeStemps(self, nt=1):
        next_ts = math.floor(self.current_ts+nt)
        n = np.searchsorted(self.ts_list[self.current_index:], next_ts)
        X, t = self.nextVectors(n)
        self.current_ts += nt
        return X, np.ceil(t).astype('int32')

    def remain_number(self):
        return len(self.ts_list) - self.current_index
    
    def analytic_error(self, Vt_hat, sig_hat, t):
        k, p = 20, 100
        V = np.empty((k, self.dim))
        for i in range(k):
            V[i] = np.random.normal(size=self.dim)
            for j in range(p):
                Vi_norm = np.linalg.norm(V[i])
                if Vi_norm == 0:
                    break
                V[i] /= np.linalg.norm(V[i])
                V[i] = self.mul_ata_btb(V[i], Vt_hat, sig_hat, t)
        _, _, V = LA.svd(V, full_matrices=False)
        return np.linalg.norm(self.mul_ata_btb(V[0], Vt_hat, sig_hat, t))

    def mul_ata_btb(self, v, Vt_hat, sig_hat, t):
        result = np.zeros(self.dim)
        index_at_t = np.searchsorted(
            self.ts_list, t)
        u, counts = np.unique(
            self.type_list[:index_at_t], return_counts=True)
        for i in range(len(u)):
            if self.dir_list[u[i]] is not None:
                result += self.dir_list[u[i]].T @ (
                    counts[i] * self.var_list[u[i]] * (self.dir_list[u[i]] @ v))
            else:
                result += dct(
                    counts[i] * self.var_list[u[i]] * dct(v, 4, norm='ortho'), 4, norm='ortho')
        return result - Vt_hat.T @ (sig_hat**2 * (Vt_hat @ v))


def uniform_normal_tsm(T, n, d, loc, scale, random_state=None):
    random_state = np.random.mtrand.RandomState(random_state)
    tsm = TimeSerialMatrix(dim=d, random_state=random_state)
    # vars = random_state.normal(size=d)**2
    vars = random_state.beta(1, 10, size=d)
    # vars /= np.linalg.norm(vars)
    if d < 2000:
        dirs = ortho_group.rvs(dim=d, random_state=random_state)
    else:
        dirs = None
    ts_list = uniform.rvs(0, T, size=n//2, random_state=random_state)
    tsm.add_type(vars=vars, dirs=dirs, ts_list=ts_list)
    # vars = random_state.normal(scale=1, size=d//10)**2
    vars = random_state.beta(1, 10, size=d//10)*10
    # vars /= np.linalg.norm(vars)
    if d < 2000:
        dirs = ortho_group.rvs(dim=d, random_state=random_state)[:d//10]
    else:
        dirs = None
        vars = np.pad(vars, (0, d-len(vars)),
                      'constant', constant_values=(0, 0))
    ts_list = norm.rvs(loc, scale, size=n//2, random_state=random_state)
    ts_list = np.clip(ts_list, 1, T)
    # ts_list = ts_list[(ts_list > 0) & (ts_list <= T)]
    tsm.add_type(vars=vars, dirs=dirs, ts_list=ts_list)
    return tsm


def old():
    n, d = 10000, 100
    random = check_random_state(0)
    bg_weight = random.beta(1, 10, size=d)
    bg_weight /= np.linalg.norm(bg_weight)
    fg_weight = random.beta(1, 5, size=d)
    fg_weight /= np.linalg.norm(fg_weight)

    alpha = .5
    skew = 0
    loc = n/5*4
    scale = n/10*3/4
    bg_ts = uniform.rvs(0, n, size=int(n*(1-alpha)), random_state=random)
    fg_ts = skewnorm.rvs(skew, loc, scale, size=int(n*alpha), random_state=random)
    bg_X = random.normal(scale=bg_weight, size=(len(bg_ts), len(bg_weight)))
    fg_X = random.normal(scale=fg_weight, size=(len(fg_ts), len(fg_weight)))

    ts = np.concatenate((bg_ts, fg_ts))
    X = np.concatenate((bg_X, fg_X), axis=0)
    order = np.argsort(ts)
    ts = ts[order]
    X = X[order]
    np.savetxt('ts.csv', ts, fmt='%.2f')
    np.savetxt('X.csv', X, fmt='%.5f')


def generate(suffix, T, n, d, loc, scale, random_state=None):
    tsm = uniform_normal_tsm(T, n, d, loc, scale,
                             random_state=random_state)

    num_dirs = len(tsm.dir_list)
    if tsm.dir_list[0] is None:
        has_dir_list = 0
    else:
        has_dir_list = 1
    
    # The ground truth file is generated for computing analytic errors.
    # It's formated as follows:
    gt_file = open(f'ground_truth_{suffix}.txt', 'w')

    # Line 1: three integers, d, num_dirs, has_dir_list
    gt_file.write(f'{d} {num_dirs} {has_dir_list}\n')

    # Next num_dirs lines: d + 1 integers, num_vecs, var_list[lineno - 2]
    for i in range(num_dirs):
        variance = tsm.var_list[i]
        gt_file.write(str(len(variance)))
        for j in range(len(variance)):
            gt_file.write(f' {variance[j]}')
        gt_file.write('\n')

    # Next batch of lines: dir_list content (only if has_dir_list == 1)
    #   in row-major format
    if has_dir_list == 1:
        for k in range(num_dirs):
            dir_mat = tsm.dir_list[k]
            for i in range(len(dir_mat)):
                gt_file.write(str(dir_mat[i, 0]))
                for j in range(1, d):
                    gt_file.write(f' {dir_mat[i, j]}')
                gt_file.write('\n')

    # Rem file: num_dirs + 1 integers, ts and num_dirs counts
    prev_ts = 0
    cnt_map = [0] * num_dirs

    f = open(f'X_{suffix}.csv', 'w')
    vec_idx = 0
    for i in range(math.ceil(n/1000)):
        X, t = tsm.nextVectors(1000)
        n = X.shape[0]
        for i in range(n):
            ts = int(math.floor(t[i] + 1))
            if ts != prev_ts:
                if prev_ts != 0:
                    gt_file.write(str(prev_ts))
                    for j in range(num_dirs):
                        gt_file.write(f' {cnt_map[j]}')
                    gt_file.write('\n')
                prev_ts = ts
            X_dir = tsm.type_list[vec_idx]
            vec_idx += 1
            cnt_map[X_dir] += 1

            f.write(str(ts)) # we require timestamp to start from 1
            for j in range(X.shape[1]):
                f.write(' %.5f' % X[i][j])
            f.write("\n")
    f.close()

    if prev_ts != 0:
        gt_file.write(str(prev_ts))
        for j in range(num_dirs):
            gt_file.write(f' {cnt_map[j]}')
        gt_file.write('\n')

    gt_file.close()

    return tsm

def small():
    generate('small', 1000, 50000, 100, 500, 20, random_state=0)


def medium():
    generate('medium', 1000, 50000, 1000, 500, 20, random_state=0)


def big():
    generate('big', 1000, 50000, 10000, 500, 20, random_state=0)


if __name__ == '__main__':
    small()
    #medium()
    #big()
