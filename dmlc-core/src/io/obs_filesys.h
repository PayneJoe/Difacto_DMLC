/*!
 *  Copyright (c) by 2019 Contributors
 * \file obs_filesys.h
 * \brief 
 * \author yuan pingzhou
 */

#ifndef DMLC_IO_OBS_FILESYS_H_
#define DMLC_IO_OBS_FILESYS_H_

#include <vector>
#include <string>
#include "./filesys.h"

namespace dmlc{
namespace io{
class ObsFileSystem : public FileSystem{
	public:
  /*! \brief destructor */
  virtual ~ObsFileSystem();
  /*!
   * \brief get information about a path
   * \param path the path to the file
   * \return the information about the file
   */
  virtual FileInfo GetPathInfo(const URI &path);
  /*!
   * \brief list files in a directory
   * \param path to the file
   * \param out_list the output information about the files
   */
  virtual void ListDirectory(const URI &path, std::vector<FileInfo> *out_list);
  /*!
   * \brief open a stream, will report error and exit if bad thing happens
   * NOTE: the Stream can continue to work even when filesystem was destructed
   * \param path path to file
   * \param uri the uri of the input
   * \param flag can be "w", "r", "a"
   * \param allow_null whether NULL can be returned, or directly report error
   * \return the created stream, can be NULL when allow_null == true and file do not exist
   */
  virtual Stream *Open(const URI &path, const char* const flag, bool allow_null);
  /*!
   * \brief open a seekable stream for read
   * \param path the path to the file
   * \param allow_null whether NULL can be returned, or directly report error
   * \return the created stream, can be NULL when allow_null == true and file do not exist
   */
  virtual SeekStream *OpenForRead(const URI &path, bool allow_null);
  /*!
   * \brief get a singleton of S3FileSystem when needed
   * \return a singleton instance
   */
  inline static ObsFileSystem *GetInstance(void) {
	static ObsFileSystem instance;
	return &instance;
  }

 private:
  /*! \brief constructor */
  ObsFileSystem();
  /*! \brief OBS access id */
  std::string obs_access_id_;
  /*! \brief OBS secret key */
  std::string obs_secret_key_;
  /*! \brief OBS cluster id*/
  std::string obs_endpoint_;

  bool TryGetPathInfo(const URI &path, FileInfo *info);
};
} // namespace io
} // namespace dmlc
#endif
