import numpy as np
from scipy.stats import beta, uniform, norm, skewnorm
from sklearn.utils import check_array, check_random_state
from scipy.stats import ortho_group, norm
from scipy.fftpack import dct
import math


class TimeSerialMatrix():
    def __init__(self, dim=100, random_state=None):
        self.dim = dim
        self.ts_list = []
        self.var_list = []
        self.dir_list = []
        self.type_list = np.empty(0, dtype=int)
        self.current_index = 0
        self.current_ts = 0
        self.random = check_random_state(random_state)

    def add_type(self, vars, dirs, ts_list):
        self.type_list = np.concatenate(
            (self.type_list, np.ones(len(ts_list), dtype=int) * len(self.var_list)))
        self.var_list.append(vars)
        self.dir_list.append(dirs)
        self.ts_list = np.concatenate((self.ts_list, ts_list))
        order = np.argsort(self.ts_list)
        self.ts_list = self.ts_list[order]
        self.type_list = self.type_list[order]

    def next_vectors(self, n=1):
        if self.current_index+n > len(self.ts_list):
            n = len(self.ts_list) - self.current_index
        if n==0:
            return np.empty((0, self.dim)), np.empty(0)
        t = self.ts_list[self.current_index:self.current_index+n]
        X = np.empty((n, self.dim), dtype=np.half)
        u, indices, counts = np.unique(
            self.type_list[self.current_index:self.current_index+n], return_inverse=True, return_counts=True)
        for i in range(len(u)):
            x = self.random.normal(scale=self.var_list[u[i]], size=(
                counts[i], len(self.var_list[u[i]]))).astype('float16')
            if self.dir_list[i] is not None:
                X[indices==i] = x @ self.dir_list[i]
            else:
                X[indices == i] = dct(x, 4, norm='ortho')
        self.current_index += n
        if self.current_index == len(self.ts_list):
            self.current_ts = self.ts_list[-1] + 1
        else:
            self.current_ts = self.ts_list[self.current_index]
        return X, t

    def nextTimeStemps(self, nt=1):
        next_ts = math.floor(self.current_ts+nt)
        n = self.current_index - np.argmax(self.ts_list > next_ts)
        X, t = self.next_vectors(n)
        return X, np.floor(t, dtype=int)

    def remain_number(self):
        return len(self.ts_list) - self.current_index


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


def generate(suffix, T, n, d, f_loc, f_scale, random_state=None):
    random_state = np.random.mtrand.RandomState(random_state)
    tsm = TimeSerialMatrix(dim=d, random_state=random_state)
    vars = random_state.beta(1, 10, size=d)
    vars /= np.linalg.norm(vars)
    if d < 2000:
        dirs = ortho_group.rvs(dim=d, random_state=random_state)
    else:
        dirs = None
    ts_list = uniform.rvs(0, T, size=n//2, random_state=random_state)
    tsm.add_type(vars=vars, dirs=dirs, ts_list=ts_list)
    vars = random_state.beta(1, 10, size=d//10)
    vars /= np.linalg.norm(vars)
    if d < 2000:
        dirs = ortho_group.rvs(dim=d, random_state=random_state)[:d//10]
    else:
        dirs = None
        vars = np.pad(vars, (0, d-len(vars)), 'constant', constant_values=(0, 0))
    ts_list = norm.rvs(f_loc, f_scale, size=n//2, random_state=random_state)
    tsm.add_type(vars=vars, dirs=dirs, ts_list=ts_list)
    open(f'X_{suffix}.csv', "w").close()
    open(f't_{suffix}.csv', "w").close()
    fX = open(f'X_{suffix}.csv', 'ab')
    ft = open(f't_{suffix}.csv', 'ab')
    for i in range(math.ceil(n/1000)):
        X, t = tsm.next_vectors(1000)
        np.savetxt(ft, t, fmt='%.2f')
        np.savetxt(fX, X, fmt='%.5f')
        ft.write(b"\n")
        fX.write(b"\n")
    fX.close()
    ft.close()


def small():
    generate('s', 100, 1000, 100, 80, 7.5, random_state=0)


def medium():
    generate('m', 1000, 10000, 1000, 800, 75, random_state=0)


def big():
    generate('b', 10000, 50000, 10000, 8000, 750, random_state=0)


if __name__ == '__main__':
    big()
