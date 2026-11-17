import numpy as np
import matplotlib.pyplot as plt

ps_2_ns = 0.001
def f(data):
    return data[data<10000000]*ps_2_ns
data = np.loadtxt("./cmake-build-release/export.csv", delimiter=",", skiprows=1)
print(data.shape)

plt.title("trigger diff")
plt.hist(f(data[:,5]), bins=1000)

plt.figure()
plt.title("delay")
plt.hist(f(data[:,4]), bins=1000)
plt.axvline(np.mean(f(data[:,5])), c="r", ls=":")


plt.figure()
H, xedges, yedges = np.histogram2d(
    data[:,2], data[:,3],
    bins=[514, 514],
    range=[[0, 514], [0, 514]]
)
plt.imshow(H, vmax=1)

plt.show()

