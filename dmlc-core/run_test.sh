#export OBS_HOME=/usr/obs
export OBS_HOME=./dep/obs
export OBS_ACCESS_KEY_ID=5AYDYYHICWBSJTDLB6KX
export OBS_SECRET_ACCESS_KEY=RcGJkwt1H5l8HqsK6yBQszrnffsT7dmaL6fd1X7h
export OBS_ENDPOINT=obs.ap-southeast-3.myhuaweicloud.com

#./yarn/run_hdfs_prog.py test/filesys_test cat obs://sprs-data-sg/NeverDelete/yuanpz/user_exploration/difactor/experiment/feed_data/20191018/train/part-00000
./yarn/run_hdfs_prog.py test/filesys_test cp ./tmp/d obs://sprs-data-sg/NeverDelete/yuanpz/tmp/cc
#./yarn/run_hdfs_prog.py test/filesys_test ls /
#./yarn/run_hdfs_prog.py test/filesys_test ls
