# wifi-sweet-spot ns3
ns3 scripts for simulating scenarios in which dynamic 802.11 frame aggregation is studied.

A previous version of this study can be found here: https://github.com/Wi5/ns3_802.11_frame_aggregation


## Citation
If you use this code, please cite the next research article:

Jose Saldana, Omer Topal, Jose Ruiz-Mas, Julian Fernandez-Navajas, "Finding the Sweet Spot for Frame Aggregation in 802.11 WLANs," IEEE Communications Letters, accepted for publication, Dec. 2020. doi: 10.1109/LCOMM.2020.3044231

https://ieeexplore.ieee.org/document/9291427

Author's self-archive version: PENDING


## Content of the repository

The `cc` folder contains the ns3 script. It has been run with ns3-30.1 (https://www.nsnam.org/releases/ns-3-30/).

`wifi-central-controlled-aggregation_v199.cc` is the main ns3 file used for the paper.

`wifi-central-controlled-aggregation_v215.cc` has also been used.

In addition, for some tests, these other versions with other coefficients have been employed:
```
wifi-central-controlled-aggregation_v199b11.cc 
wifi-central-controlled-aggregation_v199b12.cc
wifi-central-controlled-aggregation_v199b13.cc
wifi-central-controlled-aggregation_v199b14.cc
wifi-central-controlled-aggregation_v199b15.cc
wifi-central-controlled-aggregation_v199b16.cc
```

The `sh` folder contains the files used for obtaining each of the figures presented in the paper.

- Figure 1 was obtained with `test451.sh`, 1 user, seed 2.

- Figure 2a) was obtained with `test451.sh`, 1 user seed 2.

- Figure 2b) was obtained with `test452.sh`, 1 user seed 2.

- Figure 2c) was obtained with `test453.sh`, 1 user seed 2.

- Figure 3 was obtained with `test453.sh`, 1 user seed 2.

- Figures 4 to 7 were obtained with the next scripts:

`test411.sh`. No aggregation (1). Version 199

`test412.sh`. Aggregation always active (2). Version 199

`test414.sh`. Disable algorithm (3). Version 199

`test431.sh`. Method #1: linear decrease, linear increase (4). Version 199

`test521.sh`. Method #2: geometric decrease, geometric increase (5). Version 199b11

`test432.sh`. Method #3: drastic reduction to minimum, linear increase (6). Versiion 199

`test434.sh`. Method #4: linear decrease, drastic increase to maximum (7). Version 215


- Figure 8 (method #1) was obtained with the next scripts:

`test481`, `test431`, `test482`, `test483`, `test484`.

- Figure 9 (method #2) was obtained with the next scripts

`test521.sh` (Version 199b11), `test522.sh` (Version 199b12), `test523.sh` (Version 199b13), `test524.sh` (Version 199b14), `test525.sh` (Version 199b15), `test526.sh` (Version 199b16), 

## How to use it

- Download ns3.

- Put the `.cc` file in the `ns-3.30.1/scratch` directory.

- Put a `.sh` file in the `ns-3.30.1` directory.

- Run a `.sh` file. (You may need to adjust the name of the `.cc` file in the `.sh` script).
