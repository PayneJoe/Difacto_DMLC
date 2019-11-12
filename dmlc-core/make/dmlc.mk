#---------------------------------------------------------------------------------------
#  mshadow configuration script
#
#  include dmlc.mk after the variables are set
#
#  Add DMLC_CFLAGS to the compile flags
#  Add DMLC_LDFLAGS to the linker flags
#---------------------------------------------------------------------------------------- 
ifndef LIBJVM
	LIBJVM=$(JAVA_HOME)/jre/lib/amd64/server
endif

ifndef NO_OPENMP
	DMLC_CFLAGS += -fopenmp
	DMLC_LDFLAGS += -fopenmp
endif

ifndef OBS_HOME
        OBS_HOME=/usr/obs
        #OBS_HOME=./dep/obs
endif

# Mac OS X does not support "-lrt" flag
OS := $(shell uname -s)
ifneq ($(OS), Darwin)
    DMLC_LDFLAGS += -lrt
endif

# handle fpic options
ifndef WITH_FPIC
	WITH_FPIC = 1
endif

ifeq ($(WITH_FPIC), 1)
	DMLC_CFLAGS += -fPIC
endif

# Using default hadoop_home
ifndef HADOOP_HDFS_HOME
	HADOOP_HDFS_HOME=${HADOOP_HOME}
endif

ifeq ($(USE_HDFS),1)                                                      
        ifndef HDFS_INC_PATH
                HDFS_INC_PATH=$(HADOOP_HDFS_HOME)/include
        endif
        ifndef HDFS_LIB_PATH
                HDFS_LIB_PATH=$(HADOOP_HDFS_HOME)/lib/native
        endif
        #HDFS_INC_PATH=$(HADOOP_HDFS_HOME)/include
        DMLC_CFLAGS+= -DDMLC_USE_HDFS=1 -I$(HDFS_INC_PATH) -I$(JAVA_HOME)/include

        ifneq ("$(wildcard $(HDFS_LIB_PATH)/libhdfs.so)","")
                DMLC_LDFLAGS+= -lhdfs -L$(HDFS_LIB_PATH) -Wl,-rpath=$(HDFS_LIB_PATH)
        else
                DMLC_LDFLAGS+= $(HDFS_LIB_PATH)/libhdfs.a
        endif
        #HDFS_LIB_PATH=$(HADOOP_HDFS_HOME)/lib/native
        #DMLC_LDFLAGS += $(HDFS_LIB_PATH)/libhdfs.a
        DMLC_LDFLAGS += -ljvm -L$(LIBJVM) -Wl,-rpath=$(LIBJVM)
else    
        DMLC_CFLAGS+= -DDMLC_USE_HDFS=0
endif   

# setup S3
ifeq ($(USE_S3),1)
	DMLC_CFLAGS+= -DDMLC_USE_S3=1
	DMLC_LDFLAGS+= -lcurl -lssl -lcrypto
else
	DMLC_CFLAGS+= -DDMLC_USE_S3=0
endif

ifeq ($(USE_GLOG), 1)
	DMLC_CFLAGS += -DDMLC_USE_GLOG=1 -I/mnt/yuanpz/Difacto_DMLC/ps-lite/deps/include
	DMLC_LDFLAGS+= $(addprefix /mnt/yuanpz/Difacto_DMLC/ps-lite/deps/lib/, libglog.a libgflags.a)
endif

ifeq ($(USE_AZURE),1)
	DMLC_CFLAGS+= -DDMLC_USE_AZURE=1
	DMLC_LDFLAGS+= -lazurestorage
else
	DMLC_CFLAGS+= -DDMLC_USE_AZURE=0
endif

ifeq ($(USE_OBS),1)
        DMLC_CFLAGS+= -DDMLC_USE_OBS=1 -I${OBS_HOME}/include
        #DMLC_LDFLAGS+= -leSDKOBS -L./lib/obs -Wl,--rpath=./lib/obs -lsecurec -L./lib/obs
        #DMLC_LDFLAGS+= -L${OBS_HOME}/lib -leSDKOBS -Wl,-rpath-link=${OBS_HOME}/lib -lsecurec -Wl,-rpath-link=${OBS_HOME}/lib
        DMLC_LDFLAGS+= -L${OBS_HOME}/lib -leSDKOBS -lsecurec -fstack-protector --param ssp-buffer-size=4 -Wstack-protector -Wl,--disable-new-dtags,--rpath ${OBS_HOME}/lib -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fPIC
else
        DMLC_CFLAGS+= -DDMLC_USE_OBS=0
endif
