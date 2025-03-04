/* ============================================================================
 * Copyright (c) 2007-2019 BlueQuartz Software, LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of BlueQuartz Software, the US Air Force, nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The code contained herein was partially funded by the following contracts:
 *    United States Air Force Prime Contract FA8650-04-C-5229
 *    United States Air Force Prime Contract FA8650-15-D-5231
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#pragma once

#include <array>
#include <iostream>
#include <list>
#include <string>

#include <hdf5.h>
#include "H5Fpublic.h"

#include "H5Support/H5Lite.h"
#include "H5Support/H5Support.h"

/**
 * Define the libraries features and file compatibility that will be used when opening
 * or creating a file
 */
#if(H5_VERS_MINOR == 8)
#define HDF5_VERSION_LIB_LOWER_BOUNDS H5F_LIBVER_18
#define HDF5_VERSION_LIB_UPPER_BOUNDS H5F_LIBVER_LATEST
#endif

#if(H5_VERS_MINOR == 10)
#define HDF5_VERSION_LIB_LOWER_BOUNDS H5F_LIBVER_V18
#define HDF5_VERSION_LIB_UPPER_BOUNDS H5F_LIBVER_V18
#endif

#if(H5_VERS_MINOR == 12)
#define HDF5_VERSION_LIB_LOWER_BOUNDS H5F_LIBVER_V18
#define HDF5_VERSION_LIB_UPPER_BOUNDS H5F_LIBVER_V18
#endif

namespace H5Support {

#define ENABLE_BITMASK_OPERATORS(x)                                                                                                                                                                    \
  template <>                                                                                                                                                                                          \
  struct EnableBitMaskOperators<x>                                                                                                                                                                     \
  {                                                                                                                                                                                                    \
    static const bool enable = true;                                                                                                                                                                   \
  };

template <typename Enum>
struct EnableBitMaskOperators
{
  static const bool enable = false;
};

template <typename Enum>
typename std::enable_if<EnableBitMaskOperators<Enum>::enable, Enum>::type operator|(Enum lhs, Enum rhs)
{
  using underlying = typename std::underlying_type<Enum>::type;
  return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

/**
 * @brief General Utilities for working with the HDF5 data files and API
 */
namespace H5Utilities
{

enum class CustomHDFDataTypes : int32_t
{
  Group = 1,
  Dataset = 2,
  Type = 4,
  Link = 8,
  Any = 15
};

// -----------HDF5 File Operations
/**
 * @brief Opens a H5 file at path filename. Can be made read only access. Returns the id of the file object.
 * @param filename
 * @param readOnly
 * @return
 */
inline hid_t openFile(const std::string& filename, bool readOnly = false)
{
  H5SUPPORT_MUTEX_LOCK()

  HDF_ERROR_HANDLER_OFF
  hid_t fileID = -1;
  if(readOnly)
  {
    fileID = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
  }
  else
  {
    /* Create a file access property list */
    hid_t fileAccessPropertyList = H5Pcreate(H5P_FILE_ACCESS);

    /* Set the file access property list */
    H5Pset_libver_bounds(fileAccessPropertyList, HDF5_VERSION_LIB_LOWER_BOUNDS, HDF5_VERSION_LIB_UPPER_BOUNDS);
    fileID = H5Fopen(filename.c_str(), H5F_ACC_RDWR, H5P_DEFAULT);

    /* Close the file access property list object */
    H5Pclose(fileAccessPropertyList);
  }

  HDF_ERROR_HANDLER_ON
  return fileID;
}

/**
 * @brief Creates a H5 file at path filename. Returns the id of the file object.
 * @param filename
 * @return
 */
inline hid_t createFile(const std::string& filename)
{
  H5SUPPORT_MUTEX_LOCK()

  /* Create a file access property list */
  hid_t fileAccessPropertyList = H5Pcreate(H5P_FILE_ACCESS);

  /* Set the fapl */
  H5Pset_libver_bounds(fileAccessPropertyList, HDF5_VERSION_LIB_LOWER_BOUNDS, HDF5_VERSION_LIB_UPPER_BOUNDS);

  /* Create a file with this file access property list */
  hid_t fileID = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, fileAccessPropertyList);

  /* Close the file access property list object */
  H5Pclose(fileAccessPropertyList);

  return fileID;
}

/**
 * @brief Closes the object id
 * @param locId The object id to close
 * @return Negative value is error.
 */
inline herr_t closeHDF5Object(hid_t objectID)
{
  H5SUPPORT_MUTEX_LOCK()

  if(objectID < 0) // Object was not valid.
  {
    return 0;
  }
  H5I_type_t objectType;
  herr_t err = -1; // default to an error
  std::array<char, 1024> name;
  name.fill(0);
  objectType = H5Iget_type(objectID);
  ssize_t charsRead = H5Iget_name(objectID, name.data(), name.size());
  if(charsRead < 0)
  {
    std::cout << "Error Trying to get the name of an hdf object that was not closed. This is probably pretty bad. " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
    return -1;
  }

  switch(objectType)
  {
  case H5I_FILE:
    err = H5Fclose(objectID);
    break;
  case H5I_GROUP:
    // std::cout << "H5 Group Object left open. Id=" << objectID  << " Name='" << name << "'" << std::endl;
    err = H5Gclose(objectID);
    break;
  case H5I_DATASET:
    // std::cout << "H5 Dataset Object left open. Id=" << objectID << " Name='" << name << "'" << std::endl;
    err = H5Dclose(objectID);
    break;
  case H5I_ATTR:
    // std::cout << "H5 Attribute Object left open. Id=" << objectID << " Name='" << name << "'" << std::endl;
    err = H5Aclose(objectID);
    break;
  case H5I_DATATYPE:
    // std::cout << "H5 DataType Object left open. Id=" << objectID << " Name='" << name << "'" << std::endl;
    err = H5Tclose(objectID);
    break;
  case H5I_DATASPACE:
    // std::cout << "H5 Data Space Object left open. Id=" << objectID << " Name='" << name << "'" << std::endl;
    err = H5Sclose(objectID);
    break;
  default:
    // std::cout << "Error unknown HDF object for closing: " << " Name='" << name << "'" << " Object Type=" << obj_type << std::endl;
    err = -1;
  }

  return err;
}

/**
 * @brief Closes a H5 file object. Returns the H5 error code.
 * @param fileID
 * @return
 */
inline herr_t closeFile(hid_t& fileID)
{
  H5SUPPORT_MUTEX_LOCK()

  herr_t err = 1;
  if(fileID < 0) // fileID isn't open
  {
    return 1;
  }

  // Get the number of open identifiers of all types
  //  except files
  ssize_t numOpen = H5Fget_obj_count(fileID, H5F_OBJ_DATASET | H5F_OBJ_GROUP | H5F_OBJ_DATATYPE | H5F_OBJ_ATTR | H5F_OBJ_LOCAL);
  if(numOpen > 0)
  {
    std::cout << "WARNING: Some IDs weren't closed. Closing them." << std::endl;
    std::vector<hid_t> attributeIDs(numOpen, 0);
    H5Fget_obj_ids(fileID, H5F_OBJ_DATASET | H5F_OBJ_GROUP | H5F_OBJ_DATATYPE | H5F_OBJ_ATTR, numOpen, attributeIDs.data());
    std::array<char, 1024> name;
    for(const auto& id : attributeIDs)
    {
      name.fill(0);
      ssize_t charsRead = H5Iget_name(id, name.data(), name.size());
      if(charsRead < 0)
      {
        std::cout << "Error Trying to get the name of an hdf object that was not closed. This is probably pretty bad. " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
        return -1;
      }
      std::cout << "H5 Object left open. Id=" << id << " Name='" << name.data() << "'" << std::endl;
      H5Utilities::closeHDF5Object(id);
    }
  }

  err = H5Fclose(fileID);
  if(err < 0)
  {
    std::cout << "Error Closing HDF5 File. " << err << std::endl;
  }
  fileID = -1;
  return err;
}

// -------------- HDF Indentifier Methods ----------------------------
/**
 * @brief Returns the path to an object
 * @param objectID The HDF5 id of the object
 * @param trim set to False to trim the path
 * @return  The path to the object relative to the objectID
 */
inline std::string getObjectPath(hid_t locationID, bool trim = false)
{
  H5SUPPORT_MUTEX_LOCK()

  size_t nameSize = 1 + H5Iget_name(locationID, nullptr, 0);
  std::vector<char> objectName(nameSize, 0);
  H5Iget_name(locationID, objectName.data(), nameSize);
  std::string objectPath(objectName.data());

  if((objectPath != "/") && (objectPath.at(0) == '/'))
  {
    objectPath.erase(0, 1);
  }

  return objectPath;
}

/**
 * @brief Returns the hdf object type
 * @param objectID The hdf5 object id
 * @param objectName The path to the data set
 * @param objectType The type of the object
 * @return  Negative value on error
 */
inline herr_t getObjectType(hid_t objectID, const std::string& objectName, int32_t& objectType)
{
  H5SUPPORT_MUTEX_LOCK()

  herr_t error = 1;
  H5O_info_t objectInfo{};

  error = H5Oget_info_by_name(objectID, objectName.c_str(), &objectInfo, H5P_DEFAULT);
  if(error < 0)
  {
    return error;
  }

  objectType = objectInfo.type;

  return error;
}

/**
 * @brief Retrieves the object name for a given index
 * @param fileID The hdf5 object id
 * @param index The index to retrieve the name for
 * @param name The variable to store the name
 * @return Negative value is error
 */
inline herr_t objectNameAtIndex(hid_t fileID, int32_t index, std::string& name)
{
  H5SUPPORT_MUTEX_LOCK()

  ssize_t error = -1;
  // call H5Gget_objname_by_idx with name as nullptr to get its length
  ssize_t nameSize = H5Lget_name_by_idx(fileID, ".", H5_INDEX_NAME, H5_ITER_NATIVE, static_cast<hsize_t>(index), nullptr, 0, H5P_DEFAULT);
  if(nameSize < 0)
  {
    name.clear();
    return -1;
  }

  std::vector<char> buffer(nameSize + 1, 0);
  error = H5Lget_name_by_idx(fileID, ".", H5_INDEX_NAME, H5_ITER_NATIVE, static_cast<hsize_t>(index), buffer.data(), buffer.size(), H5P_DEFAULT);
  if(error < 0)
  {
    std::cout << "Error Trying to get the dataset name for index " << index << std::endl;
    name.clear(); // Make an empty string if this fails
  }
  else
  {
    name.append(buffer.data()); // Append the string to the given string
  }
  return static_cast<herr_t>(error);
}

/**
 * @brief Returns the path to an object's parent
 * @param objectPath The HDF5 path to the object
 * @param trim set to False to trim the path
 * @return  The path to the object relative to the objectID
 */
inline std::string getParentPath(const std::string& objectPath)
{
  std::string parentPath = objectPath;
  size_t start = parentPath.find_last_of('/');
  parentPath.erase(start, parentPath.size() - start);
  return parentPath;
}

/**
 * @brief Returns the path to an object's parent
 * @param objectID The HDF5 id of the object
 * @param trim set to False to trim the path
 * @return  The path to the object relative to the objectID
 */
inline std::string getParentPath(hid_t objectID)
{
  std::string objectPath = getObjectPath(objectID);
  return getParentPath(objectPath);
}

/**
 * @brief Returns the object's name from object path
 * @param objectPath The HDF5 path to the object
 * @return  The object name
 */
inline std::string getObjectNameFromPath(const std::string& objectPath)
{
  std::string str = objectPath;
  size_t end = str.find_last_of('/');
  str.erase(0, end + 1);
  return str;
}

/**
 * @brief Returns if a given hdf5 object is a group
 * @param objectID The hdf5 object that contains an object with name objectName
 * @param objectName The name of the object to check
 * @return True if the given hdf5 object id is a group
 */
inline bool isGroup(hid_t nodeID, const std::string& objectName)
{
  H5SUPPORT_MUTEX_LOCK()

  bool isGroup = true;
  herr_t error = -1;
  H5O_info_t objectInfo{};
  error = H5Oget_info_by_name(nodeID, objectName.c_str(), &objectInfo, H5P_DEFAULT);
  if(error < 0)
  {
    std::cout << "Error in methd H5Gget_objinfo" << std::endl;
    return false;
  }
  switch(objectInfo.type)
  {
  case H5O_TYPE_GROUP:
    isGroup = true;
    break;
  case H5O_TYPE_DATASET:
    isGroup = false;
    break;
  case H5O_TYPE_NAMED_DATATYPE:
    isGroup = false;
    break;
  default:
    isGroup = false;
  }
  return isGroup;
}

/**
 * @brief objectExists Returns true if an object with the name exists in the given group or file
 * @param nodeID
 * @param objectName
 * @return True if an object with the name exists
 */
inline bool objectExists(hid_t nodeID, const std::string& objectName)
{
  H5SUPPORT_MUTEX_LOCK()
  htri_t err = H5Oexists_by_name(nodeID, objectName.c_str(), H5P_DEFAULT);
  return (err > 0);
}

/**
 * @brief Opens an HDF5 object for hdf5 operations
 * @param locId the Object id of the parent
 * @param objectPath The path of the object to open
 * @return The hdf5 id of the opened object. Negative value is error.
 */
inline hid_t openHDF5Object(hid_t locationID, const std::string& objectName)
{
  H5SUPPORT_MUTEX_LOCK()

  int32_t objectType = 0;
  hid_t objectID;
  herr_t error = 0;
  HDF_ERROR_HANDLER_OFF;
  error = getObjectType(locationID, objectName, objectType);
  if(error < 0)
  {
    // std::cout << "Error: Unable to get object type for object: " << objectName << std::endl;
    HDF_ERROR_HANDLER_ON;
    return -1;
  }

  switch(objectType)
  {
  case H5O_TYPE_GROUP:
    objectID = H5Gopen(locationID, objectName.c_str(), H5P_DEFAULT);
    break;
  case H5O_TYPE_DATASET:
    objectID = H5Dopen(locationID, objectName.c_str(), H5P_DEFAULT);
    break;
  default:
    std::cout << "Unknonwn HDF Type: " << objectType << std::endl;
    objectID = -1;
  }
  HDF_ERROR_HANDLER_ON;
  return objectID;
}

/**
 * @brief Returns the associated string for the given HDF class type.
 * @param classType
 * @return
 */
inline std::string HDFClassTypeAsStr(hid_t classType)
{
  switch(classType)
  {
  case H5T_INTEGER:
    return "H5T_INTEGER";
    break;
  case H5T_FLOAT:
    return "H5T_FLOAT";
    break;
  case H5T_STRING:
    return "H5T_STRING";
    break;
  case H5T_TIME:
    return "H5T_TIME";
    break;
  case H5T_BITFIELD:
    return "H5T_BITFIELD";
    break;
  case H5T_OPAQUE:
    return "H5T_OPAQUE";
    break;
  case H5T_COMPOUND:
    return "H5T_COMPOUND";
    break;
  case H5T_REFERENCE:
    return "H5T_REFERENCE";
    break;
  case H5T_ENUM:
    return "H5T_ENUM";
    break;
  case H5T_VLEN:
    return "H5T_VLEN";
    break;
  case H5T_ARRAY:
    return "H5T_ARRAY";
    break;
  default:
    return "OTHER";
  }
}

/**
 * @brief prints the class type of the given class
 * @param classT The Class Type to print
 */
inline void printHDFClassType(H5T_class_t classType)
{
  switch(classType)
  {
  case H5T_INTEGER:
    std::cout << "H5T_INTEGER" << std::endl;
    break;
  case H5T_FLOAT:
    std::cout << "H5T_FLOAT" << std::endl;
    break;
  case H5T_STRING:
    std::cout << "H5T_STRING" << std::endl;
    break;
  case H5T_TIME:
    std::cout << "H5T_TIME" << std::endl;
    break;
  case H5T_BITFIELD:
    std::cout << "H5T_BITFIELD" << std::endl;
    break;
  case H5T_OPAQUE:
    std::cout << "H5T_OPAQUE" << std::endl;
    break;
  case H5T_COMPOUND:
    std::cout << "H5T_COMPOUND" << std::endl;
    break;
  case H5T_REFERENCE:
    std::cout << "H5T_REFERENCE" << std::endl;
    break;
  case H5T_ENUM:
    std::cout << "H5T_ENUM" << std::endl;
    break;
  case H5T_VLEN:
    std::cout << "H5T_VLEN" << std::endl;
    break;
  case H5T_ARRAY:
    std::cout << "H5T_ARRAY" << std::endl;
    break;
  default:
    std::cout << "OTHER" << std::endl;
  }
}

// -------------- HDF Group Methods ----------------------------
/**
 * @brief Returns a list of child hdf5 objects for a given object id
 * @param locationID The parent hdf5 id
 * @param typeFilter A filter to apply to the list
 * @param names Variable to store the list
 * @return
 */
inline herr_t getGroupObjects(hid_t locationID, CustomHDFDataTypes typeFilter, std::list<std::string>& names)
{
  H5SUPPORT_MUTEX_LOCK()

  herr_t error = 0;
  hsize_t numObjects = 0;
  H5G_info_t groupInfo{};
  error = H5Gget_info(locationID, &groupInfo);
  if(error < 0)
  {
    // std::cout << "Error getting number of objects for group: " << locationID << std::endl;
    return error;
  }
  numObjects = groupInfo.nlinks;

  if(numObjects <= 0)
  {
    return 0; // no objects in group
  }

  size_t size = 0;
  H5O_type_t type = H5O_TYPE_NTYPES;

  std::vector<char> name;

  for(hsize_t i = 0; i < numObjects; i++)
  {
    size = 1 + H5Lget_name_by_idx(locationID, ".", H5_INDEX_NAME, H5_ITER_INC, i, nullptr, 0, H5P_DEFAULT);

    name.resize(size, 0);

    H5Lget_name_by_idx(locationID, ".", H5_INDEX_NAME, H5_ITER_INC, i, name.data(), size, H5P_DEFAULT);
    if(typeFilter == CustomHDFDataTypes::Any)
    {
      std::string objectName(name.data());
      names.push_back(objectName);
    }
    else
    {
      H5O_info_t objectInfo{};
      error = H5Oget_info_by_name(locationID, name.data(), &objectInfo, H5P_DEFAULT);
      if(error >= 0)
      {
        type = objectInfo.type;
        if(((type == H5O_TYPE_GROUP) && ((static_cast<int32_t>(CustomHDFDataTypes::Group) & static_cast<int32_t>(typeFilter)) != 0)) ||
           ((type == H5O_TYPE_DATASET) && ((static_cast<int32_t>(CustomHDFDataTypes::Dataset) & static_cast<int32_t>(typeFilter)) != 0)))
        {
          std::string objectName(name.data());
          names.push_back(objectName);
        }
      }
    }
  }

  return error;
}

/**
 * @brief Creates a HDF Group by checking if the group already exists. If the
 * group already exists then that group is returned otherwise a new group is
 * created.
 * @param locationID The HDF unique id given to files or groups
 * @param group The name of the group to create. Note that this group name should
 * not be any sort of 'path'. It should be a single group.
 */
inline hid_t createGroup(hid_t locationID, const std::string& group)
{
  H5SUPPORT_MUTEX_LOCK()

  hid_t groupID = -1;
  herr_t error = -1;
  H5O_info_t objectInfo{};
  HDF_ERROR_HANDLER_OFF

  error = H5Oget_info_by_name(locationID, group.c_str(), &objectInfo, H5P_DEFAULT);
  //  std::cout << "H5Gget_objinfo = " << err << " for " << group << std::endl;
  if(error == 0)
  {
    groupID = H5Gopen(locationID, group.c_str(), H5P_DEFAULT);
  }
  else
  {
    groupID = H5Gcreate(locationID, group.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  }
  // Turn the HDF Error handlers back on
  HDF_ERROR_HANDLER_ON

  return groupID;
}

/**
 * @brief Given a path relative to the Parent ID, this method will create all
 * the intermediate groups if necessary.
 * @param pathToCheck The path to either create or ensure exists.
 * @param parent The HDF unique id for the parent
 * @return Error Condition: Negative is error. Positive is success.
 */
inline hid_t createGroupsFromPath(const std::string& pathToCheck, hid_t parent)
{
  H5SUPPORT_MUTEX_LOCK()

  hid_t groupID = 1;
  herr_t error = -1;
  std::string first;
  std::string second;
  std::string path(pathToCheck); // make a copy of the input

  if(parent <= 0)
  {
    std::cout << "Bad parent Id. Returning from createGroupsFromPath" << std::endl;
    return -1;
  }
  // remove any front slash
  std::string::size_type pos = path.find_first_of('/', 0);
  if(0 == pos)
  {
    path = path.substr(1, path.size());
  }
  else if(pos == std::string::npos) // Path contains only one element
  {
    groupID = H5Utilities::createGroup(parent, path);
    if(groupID < 0)
    {
      std::cout << "Error creating group: " << path << " err:" << groupID << std::endl;
      return groupID;
    }
    error = H5Gclose(groupID);
    if(error < 0)
    {
      std::cout << "Error closing group during group creation." << std::endl;
      return error;
    }
    return error; // Now return here as this was a special case.
  }

  // Remove any trailing slash
  pos = path.find_last_of('/');
  if(pos == (path.size() - 1)) // slash was in the last position
  {
    path = path.substr(0, pos);
  }

  if(path.empty())
  {
    return -1; // The path that was passed in was only a slash..
  }

  pos = path.find_first_of('/', 0);
  if(pos == std::string::npos) // Only one element in the path
  {
    groupID = H5Utilities::createGroup(parent, path);
    if(groupID < 0)
    {
      std::cout << "Error creating group '" << path << "' for group id " << groupID << std::endl;
      return groupID;
    }
    error = H5Gclose(groupID);
    return error;
  }

  while(pos != std::string::npos)
  {
    first = path.substr(0, pos);
    second = path.substr(pos + 1, path.length());
    groupID = H5Utilities::createGroup(parent, first);
    if(groupID < 0)
    {
      std::cout << "Error creating group:" << groupID << std::endl;
      return groupID;
    }
    error = H5Gclose(groupID);
    pos = path.find_first_of('/', pos + 1);
    if(pos == std::string::npos)
    {
      first += "/" + second;
      groupID = createGroup(parent, first);
      if(groupID < 0)
      {
        std::cout << "Error creating group:" << groupID << std::endl;
        return groupID;
      }
      error = H5Gclose(groupID);
    }
  }
  return error;
}

/**
 * @brief Given a path relative to the Parent ID, this method will create all
 * the intermediate groups if necessary.
 * @param datasetPath The path to the dataset that you want to make all the intermediate groups for
 * @param parent The HDF unique id for the parent
 * @return Error Condition: Negative is error. Positive is success.
 */
inline hid_t createGroupsForDataset(const std::string& datasetPath, hid_t parent)
{
  H5SUPPORT_MUTEX_LOCK()

  // Generate the internal HDF dataset path and create all the groups necessary to write the dataset
  std::string::size_type pos = 0;
  pos = datasetPath.find_last_of('/');
  // std::string parentPath;
  if(pos != 0 && pos != std::string::npos)
  {
    std::string parentPath(datasetPath.substr(0, pos));
    return H5Utilities::createGroupsFromPath(parentPath, parent);
  }
  // Make sure all the intermediary groups are in place in the HDF5 File
  return 1;
}

/**
 * @brief Extracts the object name from a given path
 * @param path The path which to extract the object name
 * @return The name of the object
 */
inline std::string extractObjectName(const std::string& path)
{
  std::string::size_type pos;
  pos = path.find_last_of('/');
  if(pos == std::string::npos || path == "/")
  {
    return path;
  }
  return path.substr(pos + 1);
}

// -------------- HDF Attribute Methods ----------------------------
/**
 * @brief Looks for an attribute with a given name
 * @param locationID The objects Parent id
 * @param objectName The name of the object
 * @param attributeName The attribute to look for (by name)
 * @return True if the attribute exists.
 */
inline bool probeForAttribute(hid_t locationID, const std::string& objectName, const std::string& attributeName)
{
  H5SUPPORT_MUTEX_LOCK()

  herr_t error = 0;
  hid_t rank;
  HDF_ERROR_HANDLER_OFF
  error = H5Lite::getAttributeNDims(locationID, objectName, attributeName, rank);
  HDF_ERROR_HANDLER_ON
  return error >= 0;
}

/**
 * @brief Returns a list of all the attribute names
 * @param objectID The parent object
 * @param names Variable to hold the list of attribute names
 * @return Negate value is error
 */
inline herr_t getAllAttributeNames(hid_t objectID, std::list<std::string>& results)
{
  H5SUPPORT_MUTEX_LOCK()

  if(objectID < 0)
  {
    return -1;
  }
  herr_t error = -1;
  hsize_t numAttributes;
  hid_t attributeID;
  size_t nameSize;
  H5O_info_t objectInfo{};
  error = H5Oget_info(objectID, &objectInfo);
  numAttributes = objectInfo.num_attrs;

  std::vector<char> attributeName;

  for(hsize_t i = 0; i < numAttributes; i++)
  {
    attributeID = H5Aopen_by_idx(objectID, ".", H5_INDEX_NAME, H5_ITER_INC, i, H5P_DEFAULT, H5P_DEFAULT);
    nameSize = 1 + H5Aget_name(attributeID, 0, nullptr);
    attributeName.resize(nameSize, 0);
    H5Aget_name(attributeID, nameSize, attributeName.data());
    results.emplace_back(attributeName.data());
    error = H5Aclose(attributeID);
  }

  return error;
}

/**
 * @brief Returns a list of all the attribute names
 * @param objectID The parent object
 * @param objectName The name of the object whose attribute names you want a list
 * @param names Variable to hold the list of attribute names
 * @return Negative value is error
 */
inline herr_t getAllAttributeNames(hid_t locationID, const std::string& objectName, std::list<std::string>& names)
{

  hid_t objectID = -1;
  herr_t error = -1;
  names.clear();

  objectID = openHDF5Object(locationID, objectName);
  if(objectID < 0)
  {
    return static_cast<herr_t>(objectID);
  }
  error = getAllAttributeNames(objectID, names);
  error = closeHDF5Object(objectID);

  return error;
}

}; // namespace H5Utilities

ENABLE_BITMASK_OPERATORS(H5Utilities::CustomHDFDataTypes)

}; // namespace H5Support
