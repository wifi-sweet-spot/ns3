#!/bin/bash

# before running this, delete the files .txt and *_average.txt
INIT_FILE_NAME="test524"

INIT_NUM_TCP_USERS=4
STEP_NUM_TCP_USERS=4
UPPER_LIMIT_NUM_TCP_USERS=20

PERCENTAGE_VOIP_USERS=100 #10 means 10%, i.e. there will be a VoIP upload user per each 10 TCP download users

INITSEED=1
MAXSEED=15

for ((i=INIT_NUM_TCP_USERS; i<=UPPER_LIMIT_NUM_TCP_USERS; i=$((${i}+${STEP_NUM_TCP_USERS})) )); do

  for ((seed=INITSEED; seed<=MAXSEED; seed++)); do

    NUMBER_TCP_USERS=$i

    NUMBER_VOIP_USERS=$(( (${NUMBER_TCP_USERS}*${PERCENTAGE_VOIP_USERS}) / 100))
    #NUMBER_VOIP_USERS=0

    #NUMBER_TCP_USERS=0

    # name of the executable file
    executablename_string=""

    # parameters of the executable
    parameters_string=""

    echo "$INIT_FILE_NAME $(date) seed: $seed. number of TCP download users $NUMBER_TCP_USERS. number VoIP upload users $NUMBER_VOIP_USERS. Starting..."

	executablename_string=${executablename_string}"scratch/wifi-central-controlled-aggregation_v199b14"

	parameters_string=${parameters_string}" --simulationTime=60 \
        --numberVoIPupload=$NUMBER_VOIP_USERS \
        --numberVoIPdownload=0 \
        --numberTCPupload=0 \
        --numberTCPdownload=$NUMBER_TCP_USERS \
        --numberVideoDownload=0 \
        --nodeMobility=2 \
	--constantSpeed=1 \
        --number_of_APs=16 \
        --number_of_APs_per_row=4 \
        --distance_between_APs=50 "

	# Use this if you are using linear mobility
        #parameters_string=${parameters_string}"--number_of_STAs_per_row=1 "

	# ARP: use this option when there is a single AP
    	#parameters_string=${parameters_string}"--arpAliveTimeout=100.0 --arpDeadTimeout=0.1 --arpMaxRetries=30 "
        # ARP: use this option when there are a number of APs and you need handoffs
        parameters_string=${parameters_string}"--arpAliveTimeout=1.0 "

	parameters_string=${parameters_string}" \
        --outputFileName=$INIT_FILE_NAME \
        --outputFileSurname="TcpDownUsers-"$NUMBER_TCP_USERS"_seed-"$seed \
        --rateModel=Ideal \
        --enablePcap=0 \
        --generateHistograms=0 \
        --writeMobility=1 \
        --numChannels=16 \
        --verboseLevel=0 \
        --printSeconds=10 \
        --version80211=1 \
        --channelWidth=20 \
        --wifiModel=1 \
        --errorRateModel=0 \
        --propagationLossModel=2 \
        --topology=2 \
        --powerLevel=-3 \
        --prioritiesEnabled=0 \
        --RtsCtsThreshold=0 "

        # Option A: No aggregation
	#parameters_string=${parameters_string}"--rateAPsWithAMPDUenabled=0.0 --aggregationDisableAlgorithm=0 "
        # Option B: Aggregation is always active
        #parameters_string=${parameters_string}"--rateAPsWithAMPDUenabled=1.0 --aggregationDisableAlgorithm=0 --aggregationDynamicAlgorithm=0 "
        # Option C: Always aggregating, defined AMPDU_max
        #parameters_string=${parameters_string}"--rateAPsWithAMPDUenabled=1.0 --aggregationDisableAlgorithm=0 --aggregationDynamicAlgorithm=0 --maxAmpduSize=8000 "
        # Option D: Detect VoIP and dissable Aggregation in the corresponding AP
        #parameters_string=${parameters_string}"--rateAPsWithAMPDUenabled=1.0 --aggregationDisableAlgorithm=1 --aggregationDynamicAlgorithm=0 "
        # Option E: Detect VoIP and reduce AMPDU_max in the corresponding AP
        #parameters_string=${parameters_string}"--rateAPsWithAMPDUenabled=1.0 --aggregationDisableAlgorithm=1 \
	#--maxAmpduSizeWhenAggregationLimited=8000 --aggregationDynamicAlgorithm=0 "
        # Option F: dynamic algorithm
        parameters_string=${parameters_string}"--rateAPsWithAMPDUenabled=1.0 --aggregationDisableAlgorithm=0 \
	--aggregationDynamicAlgorithm=1 --timeMonitorKPIs=0.25 --latencyBudget=0.004 \
	--methodAdjustAmpdu=3 --stepAdjustAmpdu=3000 "


	# print the command that is to be run. Note that \" means the quotation mark character
	echo NS_GLOBAL_VALUE=\"RngRun=$seed\" ./waf -d optimized --run \"${executablename_string}${parameters_string}\"

	# run the command
	NS_GLOBAL_VALUE="RngRun=$seed" ./waf -d optimized --run "${executablename_string}${parameters_string}"
  done
done
