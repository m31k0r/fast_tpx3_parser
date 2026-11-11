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

plt.figure()
H, xedges, yedges = np.histogram2d(
    data[:,2], data[:,3],
    bins=[514, 514],
    range=[[0, 514], [0, 514]]
)
plt.imshow(H, vmax=1)

plt.figure()
plt.hist(data[:,2], bins=1000)

plt.show()

