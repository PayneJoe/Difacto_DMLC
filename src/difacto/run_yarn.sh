#!/bin/bash

export OBS_HOME=/usr/obs
export OBS_ACCESS_KEY_ID=5AYDYYHICWBSJTDLB6KX
export OBS_SECRET_ACCESS_KEY=RcGJkwt1H5l8HqsK6yBQszrnffsT7dmaL6fd1X7h
export OBS_ENDPOINT=obs.ap-southeast-3.myhuaweicloud.com

#../../dmlc-core/tracker/dmlc_yarn.py --jobname fm_feed --mem 2024 --vcores 1 -n 12 -s 12 -f ./log4j.properties --log-level DEBUG --log-file ./fm_feed.log --env OBS_HOME=/usr/obs --env OBS_ACCESS_KEY_ID=5AYDYYHICWBSJTDLB6KX --env OBS_SECRET_ACCESS_KEY=RcGJkwt1H5l8HqsK6yBQszrnffsT7dmaL6fd1X7h --env OBS_ENDPOINT=obs.ap-southeast-3.myhuaweicloud.com /mnt/yuanpz/Difacto_DMLC/src/difacto/build/difacto.dmlc /mnt/yuanpz/Difacto_DMLC/src/difacto/guide/feed_hdfs.conf

../../dmlc-core/tracker/dmlc_yarn.py --jobname fm_feed --vcores 1 -n 12 -s 12 -f ./log4j.properties --log-level DEBUG --env OBS_HOME=/usr/obs --env OBS_ACCESS_KEY_ID=5AYDYYHICWBSJTDLB6KX --env OBS_SECRET_ACCESS_KEY=RcGJkwt1H5l8HqsK6yBQszrnffsT7dmaL6fd1X7h --env OBS_ENDPOINT=obs.ap-southeast-3.myhuaweicloud.com /mnt/yuanpz/Difacto_DMLC/src/difacto/build/difacto.dmlc /mnt/yuanpz/Difacto_DMLC/src/difacto/guide/feed_obs_pred.conf

#../../dmlc-core/tracker/dmlc_yarn.py --jobname fm_feed --vcores 8 -n 6 -s 6 -f ./log4j.properties --env OBS_HOME=/usr/obs --env OBS_ACCESS_KEY_ID=5AYDYYHICWBSJTDLB6KX --env OBS_SECRET_ACCESS_KEY=RcGJkwt1H5l8HqsK6yBQszrnffsT7dmaL6fd1X7h --env OBS_ENDPOINT=obs.ap-southeast-3.myhuaweicloud.com /mnt/yuanpz/Difacto_DMLC/src/difacto/build/dump.dmlc model_in=obs://sprs-data-sg/NeverDelete/yuanpz/user_exploration/difactor/experiment/model/fm_feed_multi_job/20191112_part-0 dump_out=obs://sprs-data-sg/NeverDelete/yuanpz/user_exploration/difactor/experiment/model/fm_feed_multi_job/dumped/part-0 need_inverse=1

#../../dmlc-core/tracker/dmlc_yarn.py --jobname fm_feed --vcores 4 -n 6 -s 6 -f ./log4j.properties --env OBS_HOME=/usr/obs --env OBS_ACCESS_KEY_ID=5AYDYYHICWBSJTDLB6KX --env OBS_SECRET_ACCESS_KEY=RcGJkwt1H5l8HqsK6yBQszrnffsT7dmaL6fd1X7h --env OBS_ENDPOINT=obs.ap-southeast-3.myhuaweicloud.com /mnt/yuanpz/Difacto_DMLC/src/difacto/build/dump.dmlc model_in=obs://sprs-data-sg/NeverDelete/yuanpz/user_exploration/difactor/experiment/model/fm_feed_multi_job/20191112_part-1 dump_out=./dumped/part-1 need_inverse=1
