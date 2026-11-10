import numpy as np
import matplotlib.pyplot as plt

data = np.loadtxt("./build/relwithdebinfo/export.csv", delimiter=",", skiprows=1)
print(data.shape)
plt.title("trigger diff")
plt.hist(data[:,5], bins=1000)
plt.figure()
plt.hist(data[:,4], bins=1000)
plt.axvline(np.mean(data[:,5]))
plt.title("delay")

plt.show()

