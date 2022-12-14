import numpy as np
import h5py as h5
import matplotlib.pyplot as plt
import sys

# for the plotting
max_r = float(sys.argv[1])  # in units of the virial radius
n_radial_bins = int(sys.argv[2])
n_snaps = int(sys.argv[3])

# some constants
OMEGA = 0.3  # Cosmological matter fraction at z = 0
PARSEC_IN_CGS = 3.0856776e18
KM_PER_SEC_IN_CGS = 1.0e5
CONST_G_CGS = 6.672e-8
CONST_m_H_CGS = 1.67e-24
h = 0.67777  # hubble parameter
gamma = 5.0 / 3.0
eta = 1.2349
H_0_cgs = 100.0 * h * KM_PER_SEC_IN_CGS / (1.0e6 * PARSEC_IN_CGS)

# read some header/parameter information from the first snapshot

filename = "CoolingHalo_0000.hdf5"
f = h5.File(filename, "r")
params = f["Parameters"]
unit_mass_cgs = float(params.attrs["InternalUnitSystem:UnitMass_in_cgs"])
unit_length_cgs = float(params.attrs["InternalUnitSystem:UnitLength_in_cgs"])
unit_velocity_cgs = float(params.attrs["InternalUnitSystem:UnitVelocity_in_cgs"])
unit_time_cgs = unit_length_cgs / unit_velocity_cgs
v_c = float(params.attrs["IsothermalPotential:vrot"])
v_c_cgs = v_c * unit_velocity_cgs
lambda_cgs = float(params.attrs["LambdaCooling:lambda_nH2_cgs"])
X_H = float(params.attrs["SPH:H_mass_fraction"])
header = f["Header"]
N = header.attrs["NumPart_Total"][0]
box_centre = np.array(header.attrs["BoxSize"])

# calculate r_vir and M_vir from v_c
r_vir_cgs = v_c_cgs / (10.0 * H_0_cgs * np.sqrt(OMEGA))
M_vir_cgs = r_vir_cgs * v_c_cgs ** 2 / CONST_G_CGS

for i in range(n_snaps):

    filename = "CoolingHalo_%04d.hdf5" % i
    f = h5.File(filename, "r")
    coords_dset = f["PartType0/Coordinates"]
    coords = np.array(coords_dset)
    # translate coords by centre of box
    header = f["Header"]
    snap_time = header.attrs["Time"]
    snap_time_cgs = snap_time * unit_time_cgs
    coords[:, 0] -= box_centre[0] / 2.0
    coords[:, 1] -= box_centre[1] / 2.0
    coords[:, 2] -= box_centre[2] / 2.0
    radius = np.sqrt(coords[:, 0] ** 2 + coords[:, 1] ** 2 + coords[:, 2] ** 2)
    radius_cgs = radius * unit_length_cgs
    radius_over_virial_radius = radius_cgs / r_vir_cgs

    r = radius_over_virial_radius

    bin_edges = np.linspace(0.0, max_r, n_radial_bins + 1)
    bin_width = bin_edges[1] - bin_edges[0]
    hist = np.histogram(r, bins=bin_edges)[0]  # number of particles in each bin

    # find the mass in each radial bin

    mass_dset = f["PartType0/Masses"]
    # mass of each particles should be equal
    part_mass = np.array(mass_dset)[0]
    part_mass_cgs = part_mass * unit_mass_cgs
    part_mass_over_virial_mass = part_mass_cgs / M_vir_cgs

    mass_hist = hist * part_mass_over_virial_mass
    radial_bin_mids = np.linspace(
        bin_width / 2.0, max_r - bin_width / 2.0, n_radial_bins
    )
    # volume in each radial bin
    volume = 4.0 * np.pi * radial_bin_mids ** 2 * bin_width

    # now divide hist by the volume so we have a density in each bin

    density = mass_hist / volume

    ##read the densities

    # density_dset = f["PartType0/Density"]
    # density = np.array(density_dset)
    # density_cgs = density * unit_mass_cgs / unit_length_cgs**3
    # rho = density_cgs * r_vir_cgs**3 / M_vir_cgs

    t = np.linspace(10.0 / n_radial_bins, 10.0, 1000)
    rho_analytic = t ** (-2) / (4.0 * np.pi)

    # calculate cooling radius

    r_cool_over_r_vir = np.sqrt(
        (2.0 * (gamma - 1.0) * lambda_cgs * M_vir_cgs * X_H ** 2)
        / (4.0 * np.pi * CONST_m_H_CGS ** 2 * v_c_cgs ** 2 * r_vir_cgs ** 3)
    ) * np.sqrt(snap_time_cgs)

    # initial analytic density profile

    if i == 0:
        r_0 = radial_bin_mids[0]
        rho_0 = density[0]

        rho_analytic_init = rho_0 * (radial_bin_mids / r_0) ** (-2)
    plt.plot(radial_bin_mids, density, "ko", label="Average density of shell")
    plt.plot(
        radial_bin_mids, rho_analytic_init, label="Initial analytic density profile"
    )
    plt.xlabel(r"$r / r_{vir}$")
    plt.ylabel(r"$(\rho / \rho_{init})$")
    plt.title(
        r"$\mathrm{Time}= %.3g \, s \, , \, %d \, \, \mathrm{particles} \,,\, v_c = %.1f \, \mathrm{km / s}$"
        % (snap_time_cgs, N, v_c)
    )
    # plt.ylim((1.e-2,1.e1))
    plt.plot(
        (r_cool_over_r_vir, r_cool_over_r_vir),
        (1.0e-4, 1.0e4),
        "r",
        label="Cooling radius",
    )
    plt.xlim((radial_bin_mids[0], max_r))
    plt.ylim((1.0e-4, 1.0e4))
    # plt.plot((0,max_r),(1,1))
    # plt.xscale('log')
    plt.yscale("log")
    plt.legend(loc="upper right")
    plot_filename = "density_profile_%03d.png" % i
    plt.savefig(plot_filename, format="png")
    plt.close()
