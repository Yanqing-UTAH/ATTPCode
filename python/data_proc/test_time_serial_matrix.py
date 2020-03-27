import unittest
from time_serial_matrix import TimeSerialMatrix, uniform_normal_tsm
import numpy as np
from scipy.stats import ortho_group, beta, uniform, skewnorm


class TestLowRankRegression(unittest.TestCase):

    def test_empty(self):
        tsm = TimeSerialMatrix()
        X, t = tsm.nextVectors()
        self.assertEqual(len(X), 0)
        self.assertEqual(len(t), 0)

    def test_n(self):
        n, d, T = 100, 10, 100
        tsm = TimeSerialMatrix(dim=d)
        vars = np.random.uniform(size=d//2)
        dirs = ortho_group.rvs(dim=d)[:d//2]
        ts_list = uniform.rvs(0, T, size=n)
        tsm.add_type(vars=vars, dirs=dirs, ts_list=ts_list)
        X, t = tsm.nextVectors(n)
        self.assertEqual(len(X), n)
        self.assertEqual(len(t), n)
        X, t = tsm.nextVectors()
        self.assertEqual(len(X), 0)
        self.assertEqual(len(t), 0)

    def test_1v(self):
        n, d, T = 100, 10, 100
        tsm = TimeSerialMatrix(dim=d)
        vars = np.random.uniform(size=d//2)
        dirs = ortho_group.rvs(dim=d)[:d//2]
        ts_list = uniform.rvs(0, T, size=n)
        tsm.add_type(vars=vars, dirs=dirs, ts_list=ts_list)
        count = 0
        while True:
            X, t = tsm.nextVectors()
            count += len(X)
            if len(X) == 0:
                break
        self.assertEqual(count, n)

    def test_1t(self):
        n, d, T = 100, 10, 100
        tsm = TimeSerialMatrix(dim=d)
        vars = np.random.uniform(size=d//2)
        dirs = ortho_group.rvs(dim=d)[:d//2]
        ts_list = uniform.rvs(0, T, size=n)
        tsm.add_type(vars=vars, dirs=dirs, ts_list=ts_list)
        count = 0
        for i in range(T):
            X, t = tsm.nextTimeStemps()
            np.testing.assert_array_equal(t, i+1)
            count += len(X)
        self.assertEqual(count, n)

    def test_random_state(self):
        n = 500
        tsm1 = uniform_normal_tsm(T=100, n=n, d=100, loc=80, scale=10,
                                  random_state=0)
        tsm2 = uniform_normal_tsm(T=100, n=n, d=100, loc=80, scale=10,
                                  random_state=0)
        tsm3 = uniform_normal_tsm(T=100, n=n, d=100, loc=80, scale=10,
                                  random_state=1)
        X1, t1 = tsm1.nextVectors(n)
        X2, t2 = tsm2.nextVectors(n)
        X3, t3 = tsm3.nextVectors(n)
        np.testing.assert_array_equal(X1, X2)
        np.testing.assert_array_equal(t1, t2)
        np.testing.assert_raises(
            AssertionError, np.testing.assert_array_equal, X1, X3)
        np.testing.assert_raises(
            AssertionError, np.testing.assert_array_equal, t2, t3)


if __name__ == '__main__':
    unittest.main()
