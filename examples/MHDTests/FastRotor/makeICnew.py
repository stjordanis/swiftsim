#############################
# This file is part of SWIFT
# Copyright
#############################

import h5py
import numpy as np
import matplotlib.pyplot as plt

# Parameters

r_in	  = 0.1
rho_in_0  = 10.
rho_out_0 = 1.
P_0	  = 1.
B_0	  = 2.5 / np.sqrt(np.pi)
omega_0   = 20.
gamma = 1.4

fileOutputName = "FastRotor.hdf5"

###---------------------------###

glass = h5py.File("glassCube_8.hdf5", 'r')

unit_cell = glass["/PartType0/Coordinates"][:, :]
h_unit_cell   = glass["/PartType0/SmoothingLength"][:] 

N_unit_cell = len(h_unit_cell)
times = 8

ratio = np.cbrt(rho_in_0/rho_out_0)

###---------------------------###

cx_out = times
cy_out = times
cz_out = 8

cx_in = int(np.ceil(ratio * cx_out))
cy_in = int(np.ceil(ratio * cy_out))
cz_in = int(np.ceil(ratio * cz_out))

lx = 1.0
ly = lx
lz = lx * float(cz_out)/float(cx_out)

print(lz)

lx_c = lx/2
ly_c = ly/2
lz_c = lz/2

lx_in = lx + (np.ceil(ratio*cx_out)/(ratio*cx_out) - 1.0)*lx
ly_in = ly + (np.ceil(ratio*cy_out)/(ratio*cy_out) - 1.0)*ly
lz_in = lz + (np.ceil(ratio*cz_out)/(ratio*cz_out) - 1.0)*lz

print(lz_in)

N_out = N_unit_cell*cx_out*cy_out*cz_out
N_in  = N_unit_cell*cx_in*cy_in*cz_in

pos_out = np.zeros((int(N_out), 3))
h_out   = np.zeros(N_out)

pos_in = np.zeros((int(N_in), 3))
h_in   = np.zeros(N_in)


c0 = 0
c1 = N_unit_cell

for i in range(0,cx_out):
	for j in range(0,cy_out):
		for k in range(0,cz_out):
			pos_out[c0:c1,0] = unit_cell[:,0]+i
			pos_out[c0:c1,1] = unit_cell[:,1]+j
			pos_out[c0:c1,2] = unit_cell[:,2]+k
			h_out[c0:c1]     = h_unit_cell[:] 
			c0 = c0 + N_unit_cell
			c1 = c1 + N_unit_cell

pos_out[:,0] = pos_out[:,0] * lx/cx_out
pos_out[:,1] = pos_out[:,1] * ly/cy_out
pos_out[:,2] = pos_out[:,2] * lz/cz_out
h_out = h_out/cx_out


c0 = 0
c1 = N_unit_cell

for i in range(0,cx_in):
	for j in range(0,cy_in):
		for k in range(0,cz_in):
			pos_in[c0:c1,0] = unit_cell[:,0]+i
			pos_in[c0:c1,1] = unit_cell[:,1]+j
			pos_in[c0:c1,2] = unit_cell[:,2]+k
			h_in[c0:c1]     = h_unit_cell[:] 
			c0 = c0 + N_unit_cell
			c1 = c1 + N_unit_cell

pos_in[:,0] = pos_in[:,0] * lx_in/cx_in
pos_in[:,1] = pos_in[:,1] * ly_in/cy_in
pos_in[:,2] = pos_in[:,2] * lz_in/cz_in
h_in = h_in/cx_in

vol = lx*ly*lz

pos_out_f = pos_out[(pos_out[:,0]-lx_c)**2 + (pos_out[:,1]-ly_c)**2 > r_in**2]
pos_in_f  = pos_in[(pos_in[:,0]-lx_c)**2 + (pos_in[:,1]-ly_c)**2 < r_in**2]

h_out_f = h_out[(pos_out[:,0]-lx_c)**2 + (pos_out[:,1]-ly_c)**2 > r_in**2]
h_in_f  = h_in[(pos_in[:,0]-lx_c)**2 + (pos_in[:,1]-ly_c)**2 < r_in**2]
h_in_f = h_in_f[pos_in_f[:,2] < lz]

pos_in_f = pos_in_f[pos_in_f[:,2] < lz]

pos = np.append(pos_out_f, pos_in_f, axis = 0)
h = np.append(h_out_f, h_in_f, axis = 0)
N = len(pos)
N_out_f = len(pos_out_f)
N_in_f = len(pos_in_f)

print(pos_out_f.shape)
print(pos_in_f.shape)
print(pos.shape)

###---------------------------###

rot = np.sqrt((pos[:,0]-lx_c)**2 + (pos[:,1]-ly_c)**2) # + (pos[:,2]-lz_c)**2)
theta = np.arctan2((pos[:,1]-ly_c), (pos[:,0]-lx_c))
v = np.zeros((N,3))
B = np.zeros((N,3)) 
ids = np.linspace(1,N,N)
m = np.ones(N) * rho_out_0 * vol / N_out
u = np.ones(N)
u[:N_out_f] = P_0 / (rho_out_0 * (gamma - 1))
u[N_out_f:] = P_0 / (rho_in_0 * (gamma - 1))

v[N_out_f:, 0] = - rot[N_out_f:] * omega_0 * np.sin(theta[N_out_f:])
v[N_out_f:, 1] =   rot[N_out_f:] * omega_0 * np.cos(theta[N_out_f:])

B[:, 0] = B_0

###---------------------------###

#File
fileOutput = h5py.File(fileOutputName, 'w')

# Header
grp = fileOutput.create_group("/Header")
grp.attrs["BoxSize"] = [lx, ly, lz] #####
grp.attrs["NumPart_Total"] =  [N, 0, 0, 0, 0, 0]
grp.attrs["NumPart_Total_HighWord"] = [0, 0, 0, 0, 0, 0]
grp.attrs["NumPart_ThisFile"] = [N, 0, 0, 0, 0, 0]
grp.attrs["Time"] = 0.0
grp.attrs["NumFileOutputsPerSnapshot"] = 1
grp.attrs["MassTable"] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
grp.attrs["Flag_Entropy_ICs"] = [0, 0, 0, 0, 0, 0]
grp.attrs["Dimension"] = 3

#Units
grp = fileOutput.create_group("/Units")
grp.attrs["Unit length in cgs (U_L)"] = 1.
grp.attrs["Unit mass in cgs (U_M)"] = 1.
grp.attrs["Unit time in cgs (U_t)"] = 1.
grp.attrs["Unit current in cgs (U_I)"] = 1.
grp.attrs["Unit temperature in cgs (U_T)"] = 1.

#Particle group
grp = fileOutput.create_group("/PartType0")
grp.create_dataset("Coordinates", data = pos, dtype = 'd')
grp.create_dataset("Velocities", data = v, dtype = 'f')
grp.create_dataset("Masses", data = m, dtype = 'f')
grp.create_dataset("SmoothingLength", data = h, dtype = 'f')
grp.create_dataset("InternalEnergy", data = u, dtype = 'f')
grp.create_dataset("ParticleIDs", data = ids, dtype = 'L')
grp.create_dataset("MagneticFluxDensity", data = B, dtype = 'f')
#grp.create_dataset("VecPot", data = vp, dtype = 'f')
#grp.create_dataset("EPalpha", data = epa, dtype = 'f')
#grp.create_dataset("EPbeta" , data = epb, dtype = 'f')

fileOutput.close()



