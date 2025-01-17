/*!
 *  Copyright (c) by 2019 Contributors
 * \file obs_filesys.cpp
 * \brief 
 * \author yuan pingzhou
 */

#include <dmlc/io.h>
#include <dmlc/logging.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <ctime>
#include <sstream>

#include "filesys.h"
#include "./obs_filesys.h"
#include "eSDKOBS.h"

namespace dmlc {
namespace io {
namespace obs {

typedef struct upload_part_callback_data
{
	int part_num; // part number
	char* put_buffer; // begining of the part data
	uint64_t buffer_size; // data length need to be posted
	uint64_t want_write_bytes;  // bytes for each write session
	uint64_t cur_offset; // current offset of being posted
	// std::string etag; // md5 for upload part
	char etag[256];
	obs_status ret_status; // return status, set when the whole part data has been posted
} upload_part_callback_data;

typedef struct get_object_callback_data
{
  char* out_buf;
  size_t want_read_bytes;
  size_t nread;
  obs_status ret_status;
}get_object_callback_data;

typedef struct list_object_callback_data
{
	int is_truncated;
	char next_marker[1024];
	int keyCount;
	int allDetails;
	obs_status ret_status;
	std::vector <FileInfo> * content;
} list_object_callback_data;

void common_error_handle(const obs_error_details *error)
{
  char errorDetails[4096];
  int len = 0;
  if (error && error->message) {
	len += snprintf(&(errorDetails[len]), sizeof(errorDetails) - len,
					"  Message: %s\n", error->message);
  }
  if (error && error->resource) {
	len += snprintf(&(errorDetails[len]), sizeof(errorDetails) - len,
					"  Resource: %s\n", error->resource);
  }
  if (error && error->further_details) {
	len += snprintf(&(errorDetails[len]), sizeof(errorDetails) - len,
					"  Further Details: %s\n", error->further_details);
  }
  if (error && error->extra_details_count) {
	len += snprintf(&(errorDetails[len]), sizeof(errorDetails) - len,
					"%s", "  Extra Details:\n");
	int i;
	for (i = 0; i < error->extra_details_count; i++) {
	  len += snprintf(&(errorDetails[len]),
					  sizeof(errorDetails) - len, "    %s: %s\n",
					  error->extra_details[i].name,
					  error->extra_details[i].value);
	}
  }
  if (len > 0) {
    LOG(FATAL) << errorDetails;
  }
  return;
}

void response_complete_callback(obs_status status,
								const obs_error_details *error,
								void *callback_data)
{
  (void) callback_data;

  if (callback_data)
  {
	obs_status *ret_status = (obs_status *)callback_data;
	*ret_status = status;
  }
  common_error_handle(error);
}

obs_status CompleteMultipartUploadCallback(const char *location,
										   const char *bucket,
										   const char *key,
										   const char* eTag,
										   void *callbackData)
{
  (void)callbackData;
  return OBS_STATUS_OK;
}

void upload_part_complete_callback(obs_status status,
								   const obs_error_details *error,
								   void *callback_data)
{
  upload_part_callback_data* data = (upload_part_callback_data*)callback_data;
  data->ret_status = status;
}

void get_object_complete_callback(obs_status status,
								  const obs_error_details *error,
								  void *callback_data)
{
  get_object_callback_data *data = (get_object_callback_data *) callback_data;
  data->ret_status = status;
}

void list_object_complete_callback(obs_status status,
								   const obs_error_details *error,
								   void *callback_data) {
    list_object_callback_data *data = (list_object_callback_data *) callback_data;
    data->ret_status = status;
}

obs_status upload_part_response_properties_callback(const obs_response_properties *properties, void *callback_data)
{
  upload_part_callback_data *data = (upload_part_callback_data*) callback_data;
  std::memcpy(data->etag, properties->etag, strlen(properties->etag));

  return OBS_STATUS_OK;
}

int upload_part_data_callback(int buffer_size, char *buffer,
								   void *callback_data)
{
  upload_part_callback_data* data = (upload_part_callback_data*) callback_data;

  if (data->want_write_bytes == 0) return 0;

  int toWrite = 0;
  if (data->want_write_bytes) { // has data
	toWrite = ((data->want_write_bytes > (unsigned) buffer_size) ?
			  (unsigned) buffer_size : data->want_write_bytes);
	memcpy(buffer, data->put_buffer + data->cur_offset, toWrite);
  }
  data->want_write_bytes -= toWrite;
  data->cur_offset += toWrite;
  // printf("upload progress %d/%d, write %d\n", data->cur_offset, data->buffer_size, toWrite);

  return toWrite;
}

obs_status get_object_data_callback(int buffer_size,
									const char *buffer,
									void *callback_data) {
  get_object_callback_data *data = (get_object_callback_data *) callback_data;
  std::memcpy(data->out_buf + data->nread, buffer, buffer_size);
  data->nread += buffer_size;
  // printf("reading progress: %d/%d\n", buffer_size, data->want_read_bytes);
  return OBS_STATUS_OK;
}

obs_status list_objects_callback(int is_truncated,
								 const char *next_marker,
								 int contents_count,
								 const obs_list_objects_content *contents,
								 int common_prefixes_count,
								 const char **common_prefixes,
								 void *callback_data) {
	list_object_callback_data *data = (list_object_callback_data *) callback_data;

	data->is_truncated = is_truncated;
	// This is tricky.  OBS doesn't return the NextMarker if there is no
	// delimiter.  Why, I don't know, since it's still useful for paging
	 // through results.  We want NextMarker to be the last content in the
	 // list, so set it to that if necessary.
	if ((!next_marker || !next_marker[0]) && contents_count) {
		next_marker = contents[contents_count - 1].key;
	}
	if (next_marker) {
		snprintf(data->next_marker, sizeof(data->next_marker), "%s",next_marker);
	}
	else {
		data->next_marker[0] = 0;
	}

	// file objects
	for (int i = 0; i < contents_count; i++) {
		const obs_list_objects_content *content = &(contents[i]);
		FileInfo info;
		URI uri;

		uri.name = std::string("/") + content->key;
		info.path = uri;
		info.size = content->size;
		info.type = kFile;
		data->content->push_back(info);
	}

	// directories
	for (int i = 0; i < common_prefixes_count; i++) {
		FileInfo info;
		URI uri;
		uri.name = std::string("/") + common_prefixes[i];
		info.path = uri;
		info.size = 0;
		info.type = kDirectory;
		data->content->push_back(info);
   	}

	data->keyCount += contents_count;

	return OBS_STATUS_OK;
}

void ListObjects(const URI& path, const std::string endpoint, const std::string obs_id, const std::string obs_key, std::vector <FileInfo> *out_list) {

  int maxkeys = 1000;

  obs_options option;
  init_obs_options(&option);

  option.bucket_options.host_name = (char*)endpoint.c_str();
  option.bucket_options.bucket_name = (char*)path.host.c_str();
  option.bucket_options.access_key = (char*)obs_id.c_str();
  option.bucket_options.secret_access_key = (char*)obs_key.c_str();

  obs_list_objects_handler list_bucket_objects_handler =
  		{
			{NULL, &list_object_complete_callback},
			&list_objects_callback
		};

  list_object_callback_data data;
  memset(&data, 0, sizeof(list_object_callback_data));
  data.allDetails = 1;
  out_list->clear();
  data.content = out_list;

  list_bucket_objects(&option, (char*)path.name.c_str() + 1, data.next_marker, "/", maxkeys, &list_bucket_objects_handler, &data);
  if(data.ret_status != OBS_STATUS_OK){
    LOG(FATAL) << obs_get_status_name(data.ret_status);
  }
  for (unsigned int i = 0;i < data.content->size();i++) {
    (*data.content)[i].path.protocol = path.protocol;
    (*data.content)[i].path.host = path.host;
  }
  return;
}

/*! \brief reader stream that can be used to read */
class ReadStream : public SeekStream {
public:
  ReadStream(const URI &path,size_t file_size)
		  : path_(path), expected_file_size_(file_size) {

    const char *keyid = getenv("OBS_ACCESS_KEY_ID");
    const char *seckey = getenv("OBS_SECRET_ACCESS_KEY");
    const char *endpoint = getenv("OBS_ENDPOINT");
    if (keyid == NULL) {
	LOG(FATAL) << "Need to set enviroment variable OBS_ACCESS_KEY_ID to use OBS";
    }
    if (seckey == NULL) {
	LOG(FATAL) << "Need to set enviroment variable OBS_SECRET_ACCESS_KEY to use OBS";
    }
    if (endpoint == NULL){
	LOG(FATAL) << "Need to set enviroment variable OBS_ENDPOINT to use OBS";
    }

    this->obs_id_ = keyid;
    this->obs_key_ = seckey;
    this->endpoint_ = endpoint;
    this->curr_bytes_ = 0;
    this->at_end_ = false;
  }

  virtual size_t Tell(void) {
  	return this->curr_bytes_;
  }
  virtual bool AtEnd(void) const {
    return this->at_end_;
  }
  virtual void Write(const void *ptr, size_t size) {
    LOG(FATAL) << "CURL.ReadStream cannot be used for write";
  }
  // lazy seek function
  virtual void Seek(size_t pos) {
    if (this->curr_bytes_ != pos) {
	  this->Cleanup();
	  this->curr_bytes_ = pos;
	}
  }

  virtual ~ReadStream(void) {
    this->Cleanup();
  }
  virtual size_t Read(void *ptr, size_t size);

private:
  // path we are reading
  URI path_;
  // endpoint
  std::string endpoint_;
  // obs access key and id
  std::string obs_id_, obs_key_;
  // size of object file
  size_t expected_file_size_;
  // current position in the stream
  size_t curr_bytes_;
  // mark end of stream
  bool at_end_;
  /*!
   * \brief cleanup the previous session for restart
   */
  void Cleanup(void);
};

// clear up stream
void ReadStream::Cleanup() {
  this->curr_bytes_ = 0;
  this->at_end_ = false;
}

size_t ReadStream::Read(void *ptr, size_t size) {
  if(size == 0) return 0;
  if(this->at_end_) return 0;

  // request object info
  obs_object_info object_info;
  memset(&object_info, 0, sizeof(object_info));
  object_info.key = (char*)path_.name.c_str() + 1;

  obs_options option;
  init_obs_options(&option);
  option.bucket_options.host_name = (char*)endpoint_.c_str();
  option.bucket_options.bucket_name = (char*)this->path_.host.c_str();
  option.bucket_options.access_key = (char*)this->obs_id_.c_str();
  option.bucket_options.secret_access_key = (char*)this->obs_key_.c_str();

  obs_get_conditions getcondition;
  memset(&getcondition, 0, sizeof(getcondition));
  init_get_properties(&getcondition);
  // The starting position of the reading
  getcondition.start_byte = this->curr_bytes_;
  // Read length, default is 0: read to the end of the object
  getcondition.byte_count = size;

  // callback data
  get_object_callback_data data;
  data.ret_status = OBS_STATUS_BUTT;
  data.out_buf = reinterpret_cast<char*>(ptr);
  data.nread = 0;
  data.want_read_bytes = size;

  obs_get_object_handler get_object_handler =
  {
  	{NULL, &get_object_complete_callback},
	  &get_object_data_callback
  };

  // printf("--- request read size %d, expected file size %d\n", size, this->expected_file_size_);

  get_object(&option, &object_info, &getcondition, 0, &get_object_handler, &data);
  if (data.ret_status == OBS_STATUS_OK) {
    this->curr_bytes_ += data.nread;

    // printf("--- read progress %d/%d, current read %d/%d\n", this->curr_bytes_, this->expected_file_size_, data.nread, size);

    if (this->curr_bytes_ == this->expected_file_size_) {
      this->at_end_ = true;
    }
    return data.nread;
  }
  else {
	LOG(FATAL) << "get " << this->path_.str() << " object failed(" << obs_get_status_name(data.ret_status) << ").";
	return 0;
  }
}

class WriteStream: public Stream {
public:
	WriteStream(const URI &path)
			: path_(path), curr_bytes_(0), write_bytes_(0) {
	  const char *keyid = getenv("OBS_ACCESS_KEY_ID");
	  const char *seckey = getenv("OBS_SECRET_ACCESS_KEY");
	  const char *endpoint = getenv("OBS_ENDPOINT");
	  if (keyid == NULL) {
		LOG(FATAL) << "Need to set enviroment variable OBS_ACCESS_KEY_ID to use OBS";
	  }
	  if (seckey == NULL) {
		LOG(FATAL) << "Need to set enviroment variable OBS_SECRET_ACCESS_KEY to use OBS";
	  }
	  if (endpoint == NULL){
		LOG(FATAL) << "Need to set enviroment variable OBS_ENDPOINT to use OBS";
	  }
	  const char *buz = getenv("DMLC_OBS_WRITE_BUFFER_MB");
	  if (buz != NULL) {
		max_buffer_size_ = static_cast<size_t>(atol(buz)) << 20UL;
	  } else {
		// 64 MB
		const size_t kDefaultBufferSize = 64 << 20UL;
		this->max_buffer_size_ = kDefaultBufferSize;
	  }

	  this->obs_id_ = keyid;
	  this->obs_key_ = seckey;
	  this->endpoint_ = endpoint;
	  this->buffer_.clear();
	  this->Init();
	}
	virtual size_t Read(void *ptr, size_t size) {
	  LOG(FATAL) << "Obs.WriteStream cannot be used for read";
	  return 0;
	}
	virtual void Write(const void *ptr, size_t size);
	// destructor
	virtual ~WriteStream() {
		this->Upload();
		this->Finish();
	}

private:
	// path we are reading
	URI path_;
	// obs access key and id
	std::string obs_id_, obs_key_, endpoint_;
	// internal maximum buffer size
	size_t max_buffer_size_;
	// maximum time of retry when error occurs
	int max_error_retry_;
	// upload_id used by obs
	std::string upload_id_;
	// write data buffer
	std::string buffer_;
	// etags of each part we uploaded
	std::vector<std::string> etags_;
	// part id of each part we uploaded
	std::vector<size_t> part_ids_;
	//
	size_t curr_bytes_, write_bytes_;
	/*!
	 * \brief initialize the upload request
	 */
	void Init(void);
	/*!
	 * \brief upload the buffer to obs, store the etag
	 * clear the buffer
	 */
	void Upload(void);
	/*!
	 * \brief commit the upload and finish the session
	 */
	void Finish(void);

};

void WriteStream::Init() {
  // Initialize option
  obs_status ret_status = OBS_STATUS_BUTT;
  obs_options option;
  init_obs_options(&option);
  option.bucket_options.host_name = (char*)this->endpoint_.c_str();
  option.bucket_options.bucket_name = (char*)this->path_.host.c_str();
  option.bucket_options.access_key = (char*)this->obs_id_.c_str();
  option.bucket_options.secret_access_key = (char*)this->obs_key_.c_str();

  char upload_id[OBS_COMMON_LEN_256] = {0};
  int upload_id_size = OBS_COMMON_LEN_256;

  obs_response_handler handler =
		  {
				  NULL,
				  &response_complete_callback
		  };

  initiate_multi_part_upload(&option, (char*)this->path_.name.c_str() + 1, upload_id_size, upload_id, NULL, 0,
							 &handler, &ret_status);
  if (OBS_STATUS_OK == ret_status)
  {
	this->upload_id_ = upload_id;
	// printf("test init upload part %s successfully. uploadId= %s\n", (char*)this->path_.name.c_str() + 1, upload_id);
  }
  else
  {
	LOG(FATAL) << "test init upload part for " << this->path_.str() <<  " failed(" << obs_get_status_name(ret_status) << ").";
  }
}

void WriteStream::Upload() {
  // option
  obs_options option;
  init_obs_options(&option);
  option.bucket_options.host_name = (char*)this->endpoint_.c_str();
  option.bucket_options.bucket_name = (char*)this->path_.host.c_str();
  option.bucket_options.access_key = (char*)this->obs_id_.c_str();
  option.bucket_options.secret_access_key = (char*)this->obs_key_.c_str();
  option.request_options.upload_buffer_size = 2 * 1024 * 1024;

  // put properties
  obs_put_properties putProperties={0};
  init_put_properties(&putProperties);

  // upload part info
  obs_upload_part_info uploadPartInfo;
  memset(&uploadPartInfo, 0, sizeof(obs_upload_part_info));
  uploadPartInfo.part_number= this->etags_.size() + 1;
  uploadPartInfo.upload_id= (char*)this->upload_id_.c_str();

  // callback data
  upload_part_callback_data data;
  memset(&data, 0, sizeof(upload_part_callback_data));
  data.part_num = this->etags_.size() + 1;
  data.put_buffer = (char*)this->buffer_.c_str();
  data.buffer_size = this->buffer_.length(); // not necessarily being equal max_buffer_size_
  data.cur_offset = 0;
  data.want_write_bytes = this->buffer_.length();

  // callback handler
  obs_upload_handler Handler =
		  {
				  {&upload_part_response_properties_callback, &upload_part_complete_callback},
				  &upload_part_data_callback
		  };

  upload_part(&option, (char*)this->path_.name.c_str() + 1, &uploadPartInfo, data.buffer_size, &putProperties, 0, &Handler, &data);
  if (OBS_STATUS_OK == data.ret_status) {
	this->curr_bytes_ += data.cur_offset + 1;
	this->buffer_.clear();
	this->part_ids_.push_back(data.part_num);
	CHECK(strlen(data.etag) != 0);
	this->etags_.push_back(data.etag);
  }
  else
  {
	LOG(FATAL) << "upload part %d " << data.part_num << " for " << this->path_.str() << " failed(" <<  obs_get_status_name(data.ret_status) << ").";
  }
}

void WriteStream::Finish() {
  CHECK(this->etags_.size() == this->part_ids_.size());

  obs_status ret_status = OBS_STATUS_BUTT;
  // upload info
  obs_complete_upload_Info info[this->part_ids_.size()];
  for (unsigned int i = 0;i < this->part_ids_.size();i++) {
	info[i].part_number= this->part_ids_[i];
	info[i].etag= (char*)this->etags_[i].c_str();
  }

  // option
  obs_options option;
  init_obs_options(&option);
  option.bucket_options.host_name = (char*)this->endpoint_.c_str();
  option.bucket_options.bucket_name = (char*)this->path_.host.c_str();
  option.bucket_options.access_key = (char*)this->obs_id_.c_str();
  option.bucket_options.secret_access_key = (char*)this->obs_key_.c_str();

  // properties
  obs_put_properties putProperties={0};
  init_put_properties(&putProperties);

  // callback
  obs_complete_multi_part_upload_handler Handler =
		  {
				  {NULL,
				   &response_complete_callback},
				  &CompleteMultipartUploadCallback
		  };

  complete_multi_part_upload(&option, (char*)this->path_.name.c_str() + 1, (char*)this->upload_id_.c_str(), this->part_ids_.size(), info, &putProperties,
							 &Handler, &ret_status);
  if (OBS_STATUS_OK != ret_status) {
    LOG(FATAL) << "test complete upload for " <<  this->path_.str() << " failed(" << obs_get_status_name(ret_status) << ").";
  }
}

void WriteStream::Write(const void *ptr, size_t size) {
  // printf("++++++ current progress %d/%d\n", (int)this->curr_bytes_, (int)this->write_bytes_);
  size_t rlen = this->buffer_.length();
  this->buffer_.resize(rlen + size);
  std::memcpy(BeginPtr(this->buffer_) + rlen, ptr, size);
  this->write_bytes_ += size;
  if (this->buffer_.length() >= this->max_buffer_size_) {
	this->Upload();
  }
}
}; // end of obs

ObsFileSystem::ObsFileSystem() {
	const char *keyid = getenv("OBS_ACCESS_KEY_ID");
	const char *seckey = getenv("OBS_SECRET_ACCESS_KEY");
	const char *endpoint = getenv("OBS_ENDPOINT");
	if (keyid == NULL) {
		LOG(FATAL) << "Need to set enviroment variable OBS_ACCESS_KEY_ID to use OBS";
	}
	if (seckey == NULL) {
		LOG(FATAL) << "Need to set enviroment variable OBS_SECRET_ACCESS_KEY to use OBS";
	}
	if (endpoint == NULL){
		LOG(FATAL) << "Need to set enviroment variable OBS_ENDPOINT to use OBS";
	}
	obs_access_id_ = keyid;
	obs_secret_key_ = seckey;
	obs_endpoint_ = endpoint;

	// initialize obs sdk
	obs_initialize(OBS_INIT_ALL);
	set_online_request_max_count(128);
}

ObsFileSystem::~ObsFileSystem() {
	/*------ deinitialize------*/
	obs_deinitialize();
}

bool ObsFileSystem::TryGetPathInfo(const URI &path_, FileInfo *out_info) {
  URI path = path_;
  // remove slash at tail
  while (path.name.length() > 1 &&
		 *path.name.rbegin() == '/') {
	path.name.resize(path.name.length() - 1);
  }
  std::vector<FileInfo> files;
  obs::ListObjects(path, obs_endpoint_, obs_access_id_, obs_secret_key_, &files);
  std::string pdir = path.name + '/';
  for (size_t i = 0; i < files.size(); ++i) {
	if (files[i].path.name == path.name) {
	  *out_info = files[i]; return true;
	}
	if (files[i].path.name == pdir) {
	  *out_info = files[i]; return true;
	}
  }
  return false;
}

FileInfo ObsFileSystem::GetPathInfo(const URI &path) {
  CHECK(path.protocol == "obs://")
		  << " ObsFileSystem.ListDirectory";
  FileInfo info;
  CHECK(TryGetPathInfo(path, &info))
		  << "ObsFileSytem.GetPathInfo cannot find information about " + path.str();
  return info;
}

void ObsFileSystem::ListDirectory(const URI &path, std::vector <FileInfo> *out_list) {
	CHECK(path.protocol == "obs://") << " ObsFileSystem.ListDirectory";
  	if (path.name[path.name.length() - 1] == '/') {
		obs::ListObjects(path, obs_endpoint_, obs_access_id_, obs_secret_key_, out_list);
		return;
  	}

  	std::vector<FileInfo> files;
  	std::string pdir = path.name + '/';
  	out_list->clear();
  	obs::ListObjects(path, obs_endpoint_,obs_access_id_, obs_secret_key_, &files);
	for (size_t i = 0; i < files.size(); ++i) {
		if (files[i].path.name == path.name) {
	 		CHECK(files[i].type == kFile);
	  		out_list->push_back(files[i]);
	  		return;
		}
		if (files[i].path.name == pdir) {
	 		CHECK(files[i].type == kDirectory);
	  		obs::ListObjects(files[i].path, obs_endpoint_, obs_access_id_,obs_secret_key_, out_list);
	  		return;
		}
  	}
}

Stream *ObsFileSystem::Open(const URI &path, const char *const flag, bool allow_null) {
  using namespace std;
  if (!strcmp(flag, "r") || !strcmp(flag, "rb")) {
    return OpenForRead(path, allow_null);
  } else if (!strcmp(flag, "w") || !strcmp(flag, "wb")) {
    CHECK(path.protocol == "obs://") << " ObsFileSystem.Open";
    return new obs::WriteStream(path);
  } else {
    LOG(FATAL) << "ObsFileSytem.Open do not support flag " << flag;
    return NULL;
  }
}

SeekStream *ObsFileSystem::OpenForRead(const URI &path, bool allow_null) {
  CHECK(path.protocol == "obs://") << " ObsFileSystem.Open";
  FileInfo info;
  if (TryGetPathInfo(path, &info) && info.type == kFile) {
    return new obs::ReadStream(path, info.size);
  } else {
    CHECK(allow_null) << " ObsFileSystem: fail to open \"" << path.str() << "\"";
    return NULL;
  }
}
} // end of io
} // end of dmlc
