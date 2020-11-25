# wifi-sweet-spot ns3
ns3 scripts for simulating scenarios in which dynamic 802.11 frame aggregation is studied.

A previous version of this study can be found here: https://github.com/Wi5/ns3_802.11_frame_aggregation


## Citation
If you use this code, please cite the next research article:

PENDING

http://ieeexplore.ieee.org/document/ PENDING

Author's self-archive version: PENDING

`wifi-central-controlled-aggregation_v199b.cc` is the ns3 file used for the paper.
In addition, for some tests, these other versions with other coefficients have been employed:
`wifi-central-controlled-aggregation_v199b11.cc` 
`wifi-central-controlled-aggregation_v199b12.cc`
`wifi-central-controlled-aggregation_v199b13.cc`
`wifi-central-controlled-aggregation_v199b14.cc`
`wifi-central-controlled-aggregation_v199b15.cc`
`wifi-central-controlled-aggregation_v199b16.cc`

## Content of the repository

The `.cc` file contains the ns3 script. It has been run with ns3-30.1 (https://www.nsnam.org/releases/ns-3-30/).

The folder `shell_scripts_used_in_the_paper` contains the files used for obtaining each of
the figures presented in the paper.

- Figure 1 was obtained with `test451sh`, 1 user, seed 2.

- Figure 2a) was obtained with `test451.sh`, 1 user seed 2.

- Figure 2b) was obtained with `test452.sh`, 1 user seed 2.

- Figure 2c) was obtained with `test453.sh`, 1 user seed 2.

- Figure 3 was obtained with `test453.sh`, 1 user seed 2.

- Figures 4 to 7 were obtained with the next scripts:
`test411.sh`. No aggregation (1)
`test412.sh`. Aggregation always active (2)
`test414.sh`. Disable algorithm (3)
`test431.sh -> maybe test521.sh`
`test432.sh`
`test433.sh`
`test434.sh`


- Figure 5
```
test_fig5.1_5-25.sh	No aggregation				5, 10, 15, 20 users
test_fig5.1_40-50.sh	No aggregation				40, 50 users
test_fig5.1_80.sh	No aggregation				80 users
test_fig5.2_5-25.sh	AMPDU aggregation			5, 10, 15, 20 users
test_fig5.2_40-50.sh	AMPDU aggregation			40, 50 users
test_fig5.2_80.sh	AMPDU aggregation			80 users
test_fig5.3_5-25.sh	AMPDU aggregation, 8kB			5, 10, 15, 20 users
test_fig5.3_40-50.sh	AMPDU aggregation, 8kB			40, 50 users
test_fig5.3_80.sh	AMPDU aggregation, 8kB			80 users
test_fig5.4_5-25.sh	AMPDU aggregation, Algorithm		5, 10, 15, 20 users
test_fig5.4_40-50.sh	AMPDU aggregation, Algorithm		40, 50 users
test_fig5.4_80.sh	AMPDU aggregation, Algorithm		80 users
test_fig5.5_5-25.sh	AMPDU aggregation, 8kB, Algorithm	5, 10, 15, 20 users
test_fig5.5_40-50.sh	AMPDU aggregation, 8kB, Algorithm	40, 50 users
test_fig5.5_80.sh	AMPDU aggregation, 8kB, Algorithm	80 users
```

## How to use it

- Download ns3.

- Put the `.cc` file in the `ns-3.26/scratch` directory.

- Put a `.sh` file in the `ns-3.26` directory.

- Run a `.sh` file. (You may need to adjust the name of the `.cc` file in the `.sh` script).
