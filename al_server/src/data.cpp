#include <cstdlib>
#include <cstring>
#include <set>
#include <sys/utsname.h>
#include <ctime>

#include "base_config.h"
#include "data.h"


using namespace std;



MData::MData(void) :
m_numObjs(0),
m_haveLabels(false),
m_objects(NULL),
m_labels(NULL),
m_xCentroid(NULL),
m_yCentroid(NULL),
m_slideIdx(NULL),
m_slides(NULL),
m_haveDbIds(false),
m_dbIds(NULL),
m_haveIters(false),
m_iteration(NULL),
m_means(NULL),
m_stdDevs(NULL),
m_created(false)
{

}






MData::~MData(void)
{
	Cleanup();
}





void MData::Cleanup(void)
{
	if( m_objects ) {
		if( m_objects[0] )
			free(m_objects[0]);
		free(m_objects);
		m_objects = NULL;
	}

	if( m_labels ) {
		free(m_labels);
		m_labels = NULL;
	}
	if( m_xCentroid ) {
		free(m_xCentroid);
		m_xCentroid = NULL;
	}
	if( m_yCentroid ) {
		free(m_yCentroid);
		m_yCentroid = NULL;
	}
	if( m_slideIdx ) {
		free(m_slideIdx);
		m_slideIdx = NULL;
	}

	if( m_dbIds ) {
		free(m_dbIds);
		m_dbIds = NULL;
	}

	if( m_iteration ) {
		free(m_iteration);
		m_iteration = NULL;
	}

	if( m_means ) {
		free(m_means);
		m_means = NULL;
	}

	if( m_stdDevs ) {
		free(m_stdDevs);
		m_stdDevs = NULL;
	}

	// If data was loaded from an existing file, HDF5 will allocate the
	// buffers for the variable length strings (sides dataset). If we
	// used the create method to load data, we must free each string
	// buffer.
	//
	if( m_created ) {
		for(int i = 0; i < m_numSlides; i++) {
			free(m_slides[i]);
		}
		free(m_slides);
		m_slides = NULL;
	} else {
		if( m_slides ) {
			// Release mem allocated by H5Dread
			H5Dvlen_reclaim(m_memType, m_space, H5P_DEFAULT, m_slides);
			free(m_slides);
			m_slides = NULL;
		}
	}
	m_numSlides = 0;
}






//
//	Loads the data from an HDF5 file. There should be 2 datasets:
//	/features and /labels. The features data should be 2D in row-major
//	order, with each row representing an object.
//
//
bool MData::Load(string fileName)
{
	bool		result = true;
	hid_t		fileId;
	hsize_t		dims[2];
	herr_t		status;

	fileId = H5Fopen(fileName.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if( fileId < 0 ) {
		result = false;
	}

	if( result ) {
		status = H5LTget_dataset_info(fileId, "/features", dims, NULL, NULL);
		if( status < 0 ) {
			result = false;
		}
	}

	// Allocate buffer for the labels (if there are any)
	if( result && H5Lexists(fileId, "/labels", H5P_DEFAULT) ) {

		m_labels = (int*)malloc(dims[0] * sizeof(int));
		if( m_labels == NULL ) {
			result = false;
		}

		// Read label data
		if( result ) {
			status = H5LTread_dataset_int(fileId, "/labels", m_labels);
			if( status < 0 ) {
				result = false;
			} else {
				m_haveLabels = true;
			}
		}
	}

	bool slidesExist = H5Lexists(fileId, "/slides", H5P_DEFAULT);
	// Allocate buffer for slide indices
	//
	if( result && slidesExist ) {

		m_slideIdx = (int*)malloc(dims[0] * sizeof(int));
		if( m_slideIdx == NULL ) {
			result = false;
		}

		// Read slide indices
		if( result ) {
			status = H5LTread_dataset_int(fileId, "/slideIdx", m_slideIdx);
			if( status < 0 ) {
				result = false;
			}
		}
	}

	if( result && H5Lexists(fileId, "/x_centroid", H5P_DEFAULT) ) {
		
		// Allocate buffer for x_centroid
		//
		if( result ) {
			m_xCentroid = (float*)malloc(dims[0] * sizeof(float));
			if( m_xCentroid == NULL ) {
				result = false;
			}
		}
		// Read x centroids
		if( result ) {
			status = H5LTread_dataset_float(fileId, "/x_centroid", m_xCentroid);
			if( status < 0 ) {
				result = false;
			}
		}

		// Allocate buffer for y centroids
		//
		if( result ) {
			m_yCentroid = (float*)malloc(dims[0] * sizeof(float));
			if( m_yCentroid == NULL ) {
				result = false;
			}
		}

		// Read y centroids
		if( result ) {
			status = H5LTread_dataset_float(fileId, "/y_centroid", m_yCentroid);
			if( status < 0 ) {
				result = false;
			}
		}
	}
	// Allocate a buffer for the feature data
	//
	if( result ) {

		m_objects = (float**)malloc(dims[0] * sizeof(float*));
		if( m_objects ) {
			m_objects[0] = (float*)malloc(dims[0] * dims[1] * sizeof(float));
			if( m_objects[0] ) {
				for(int i = 1; i < dims[0]; i++)
					m_objects[i] = m_objects[i - 1] + dims[1];
			} else {
				result = false;
			}
		} else {
			result = false;
		}
	}

	// Read feature data
	//
	if( result ) {

		status = H5LTread_dataset_float(fileId, "/features", m_objects[0]);
		if( status < 0 ) {
			result = false;
		} else {
			m_numObjs = dims[0];
			m_numDim = dims[1];
		}
	}

	// Get means
	//
	if( result ) {

		// Allocate buffer
		m_means = (float*)malloc(dims[1] * sizeof(float));
		if( m_means == NULL ) {
			result = false;
		} else {

			status = H5LTread_dataset_float(fileId, "/mean", m_means);
			if( status < 0 ) {
				result = false;
			}

		}
	}

	// Get standard deviations
	//
	if( result ) {

		// Allocate buffer
		m_stdDevs = (float*)malloc(dims[1] * sizeof(float));
		if( m_stdDevs == NULL ) {
			result = false;
		} else {
			status = H5LTread_dataset_float(fileId, "/std_dev", m_stdDevs);
			if( status < 0 ) {
				result = false;
			}
		}
	}

	if( slidesExist ) {

		// Read slide names, do this last because we reuse the dims variable
		//
		hid_t	dset, fileType;
		if( result ) {
			dset = H5Dopen(fileId, "slides", H5P_DEFAULT);
			fileType = H5Dget_type(dset);
			m_space = H5Dget_space(dset);
			H5Sget_simple_extent_dims(m_space, dims, NULL);

			m_slides = (char**)malloc(dims[0] * sizeof(char*));
			if( m_slides == NULL ) {
				result = false;
			}
		}

		if( result ) {
			m_memType = H5Tcopy(H5T_C_S1);
			H5Tset_size(m_memType, H5T_VARIABLE);
			status = H5Dread(dset, m_memType, H5S_ALL, H5S_ALL, H5P_DEFAULT, m_slides);
			if( status < 0 ) {
				result = false;
			} else {
				m_numSlides = dims[0];
				H5Dclose(dset);
				H5Tclose(fileType);
			}
		}

		if( result && H5Lexists(fileId, "/dataIdx", H5P_DEFAULT) ) {
			m_dataIdx = (int*)malloc(m_numSlides * sizeof(int));
			if( m_dataIdx == NULL ) {
				result = false;
			} else {
				status = H5LTread_dataset_int(fileId, "/dataIdx", m_dataIdx);
				if( status < 0 ) {
					result = false;
				}
			}
		}
	}

	if( fileId > 0 )
		H5Fclose(fileId);

	return result;
}







int MData::FindItem(float xCent, float yCent, string slide)
{
	int		idx = -1, slideIdx = -1;

	for(int i = 0; i < m_numSlides; i++) {
		if( slide.compare(m_slides[i]) == 0 ) {
			slideIdx = i;
			break;
		}
	}

	if( slideIdx != -1 ) {
		for(int i = 0; i < m_numObjs; i++) {
			if( xCent == m_xCentroid[i] &&
				yCent == m_yCentroid[i] &&
				slideIdx == m_slideIdx[i] ) {

				idx = i;
				break;
			}
		}
	}
	return idx;
}





bool MData::GetSample(int index, float *sample)
{
	bool	result = false;

	if( index < m_numObjs ) {
		memcpy(sample, m_objects[index], m_numDim * sizeof(float));
		result = true;
	}
	return result;
}





char *MData::GetSlide(int index)
{
	char	*slide = NULL;

	if( index < m_numObjs ) {
		int	slideIdx = m_slideIdx[index];
		slide = m_slides[slideIdx];
	}
	return slide;
}




float MData::GetXCentroid(int index)
{
	float	xCent = -1.0;

	if( index < m_numObjs ) {
		xCent = m_xCentroid[index];
	}
	return xCent;
}



float MData::GetYCentroid(int index)
{
	float	yCent = -1.0;

	if( index < m_numObjs ) {
		yCent = m_yCentroid[index];
	}
	return yCent;
}





bool MData::Create(float *dataSet, int numObjs, int numDims, int *labels,
					int *ids, int *iteration, float *means, float *stdDev,
					float *xCentroid, float *yCentroid, char **slides,
					int *slideIdx, int numSlides)
{
	bool 	result = true;
	float	**dataSetIdx = NULL;

	// Make sure all resources have been released
	Cleanup();

	m_numObjs = numObjs;
	m_numDim = numDims;
	m_created = true;

	// Allocate buffer for objects
	m_objects = (float**)malloc(numObjs * sizeof(float*));
	if( m_objects != NULL ) {
		m_objects[0] = (float*)malloc(numObjs * numDims * sizeof(float));

		if( m_objects[0] != NULL ) {
			memcpy(m_objects[0], dataSet, numObjs * numDims * sizeof(float));

			for(int i = 1; i < numObjs; i++) {
				m_objects[i] = m_objects[i - 1] + numDims;
			}
		} else {
			result = false;
		}
	} else {
		result = false;
	}

	if( result && labels != NULL ) {
		m_labels = (int*)malloc(numObjs * sizeof(int));
		if( m_labels != NULL ) {
			m_haveLabels = true;
			memcpy(m_labels, labels, numObjs * sizeof(int));
		} else {
			result = false;
		}
	}

	if( result && ids != NULL ) {

		m_dbIds = (int*)malloc(numObjs * sizeof(int));
		if( m_dbIds != NULL ) {
			m_haveDbIds = true;
			memcpy(m_dbIds, ids, numObjs * sizeof(int));
		} else {
			result = false;
		}
	}

	if( result && iteration != NULL ) {
		m_iteration = (int*)malloc(numObjs * sizeof(int));
		if( m_iteration != NULL ) {
			m_haveIters = true;
			memcpy(m_iteration, iteration, numObjs * sizeof(int));
		} else {
			result = false;
		}
	}

	if( result && means != NULL ) {
		m_means = (float*)malloc(numObjs * sizeof(float));
		if( m_means != NULL ) {
			memcpy(m_means, means, numObjs * sizeof(float));
		} else {
			result = false;
		}
	}

	if( result && stdDev != NULL ) {
		m_stdDevs = (float*)malloc(numObjs * sizeof(float));
		if( m_stdDevs != NULL ) {
			memcpy(m_stdDevs, stdDev, numObjs * sizeof(float));
		} else {
			result = false;
		}
	}

	if( result && xCentroid != NULL ) {
		m_xCentroid = (float*)malloc(numObjs * sizeof(float));
		if( m_xCentroid != NULL ) {
			memcpy(m_xCentroid, xCentroid, numObjs * sizeof(float));
		} else {
			result = false;
		}
	}

	if( result && yCentroid != NULL ) {
		m_yCentroid = (float*)malloc(numObjs * sizeof(float));
		if( m_yCentroid != NULL ) {
			memcpy(m_yCentroid, yCentroid, numObjs * sizeof(float));
		} else {
			result = false;
		}
	}

	if( result ) {
		result = CreateSlideData(slides, slideIdx, numSlides, numObjs);
	}

	return result;
}





bool MData::CreateSlideData(char **slides, int *slideIdx, int numSlides, int numObjs)
{
	bool result = true;
	set<int>	usedSlides;
	set<int>::iterator it;
	int			*crossRef = NULL, *newSlideList = NULL;
	char		**newSlides = NULL;

	// slides may be a superset of what is actually needed. Iterate through slideIdx
	// to get the slides that are actually needed.
	//
	for(int i = 0; i < numObjs; i++) {
		usedSlides.insert(slideIdx[i]);
	}
	
	// Original slide idx, new index is crossRef[idx]
	//
	crossRef = (int*)malloc(numSlides * sizeof(int));
	newSlideList = (int*)malloc(numObjs * sizeof(int));
	if( crossRef == NULL || newSlideList == NULL ) {
		result = false;
	}

	// Store the new index at the position of the old index
	int pos = 0;
	for(it = usedSlides.begin(); it != usedSlides.end(); it++) {
		if( *it >= numSlides ) {
			result = false;
			break;
		}
		crossRef[*it] = pos++;
	}

	// Allocate memory for the new string pointers
	if( result ) {
		newSlides = (char**)malloc(usedSlides.size() * sizeof(char*));
		if( newSlides == NULL ) {
			result = false;
		}
	}

	// Copy the slide names
	if( result ) {
		for(it = usedSlides.begin(); it != usedSlides.end(); it++) {
			pos = crossRef[*it];
			newSlides[pos] = (char*)malloc(strlen(slides[*it]));
			if( newSlides[pos] == NULL ) {
				result = false;
				break;
			}
			strcpy(newSlides[pos], slides[*it]);
		}
	}

	// Create slide index
	if( result ) {
		for(int i = 0; i < numObjs; i++) {
			newSlideList[i] = crossRef[slideIdx[i]];
		}
	}

	if( result ) {
		m_slides = newSlides;
		m_slideIdx = newSlideList;
		m_numSlides = usedSlides.size();
	}
	
	if( crossRef ) 
		free(crossRef);
		
	return result;
}





bool MData::SaveAs(string filename)
{
	bool	result = true;
	hid_t	fileId;
	hsize_t	dims[2];
	herr_t	status;

	fileId = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	if( fileId < 0 ) {
		result = false;
	}

	if( result ) {
		dims[0] = m_numObjs;
		dims[1] = m_numDim;
		status = H5LTmake_dataset(fileId, "/features", 2, dims, H5T_NATIVE_FLOAT, m_objects[0]);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/labels", 2, dims, H5T_NATIVE_INT, m_labels);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/db_id", 2, dims, H5T_NATIVE_INT, m_dbIds);
		if( status < 0 ) {
			result = false;
		}
	}

	if( m_haveIters && result ) {
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/sample_iter", 2, dims, H5T_NATIVE_INT, m_iteration);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/x_centroid", 2, dims, H5T_NATIVE_FLOAT, m_xCentroid);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/y_centroid", 2, dims, H5T_NATIVE_FLOAT, m_yCentroid);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/slideIdx", 2, dims, H5T_NATIVE_INT, m_slideIdx);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		hsize_t size = m_numSlides;
		hid_t	dSpace = H5Screate_simple(1, &size, NULL),
				dType, dataSet;
		if( dSpace < 0 ) {
			result = false;
		} else {
			dType = H5Tcopy(H5T_C_S1);
			H5Tset_size(dType, H5T_VARIABLE);
			dataSet = H5Dcreate(fileId, "slides", dType, dSpace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
			if( dataSet < 0 ) {
				result = false;
			} else {
				H5Dwrite(dataSet, dType, dSpace, H5S_ALL, H5P_DEFAULT, m_slides);
				H5Dclose(dataSet);
				H5Sclose(dSpace);
				H5Tclose(dType);
			}
		}
	}

	if( result ) {
		dims[0] = m_numDim;
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/mean", 2, dims, H5T_NATIVE_FLOAT, m_means);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		dims[0] = m_numDim;
		dims[1] = 1;
		status = H5LTmake_dataset(fileId, "/std_dev", 2, dims, H5T_NATIVE_FLOAT, m_stdDevs);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result )
		result = SaveProvenance(fileId);

	if( fileId >= 0 ) {
		H5Fclose(fileId);
	}
	return result;
}






bool MData::SaveProvenance(hid_t fileId)
{
	bool	result = true;
	hsize_t	dims[2];
	herr_t	status;

	struct utsname	hostInfo;
	if( uname(&hostInfo) ) {
		result = false;
	}

	if( result ) {
		string sysInfo = hostInfo.nodename;
		sysInfo += ", ";
		sysInfo += hostInfo.sysname;
		sysInfo += " (";
		sysInfo += hostInfo.release;
		sysInfo += " ";
		sysInfo += hostInfo.machine;
		sysInfo += ")";

		status = H5LTset_attribute_string(fileId, "/", "host info", sysInfo.c_str());
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		int ver[2] = {AL_SERVER_VERSION_MAJOR, AL_SERVER_VERSION_MINOR};

		status = H5LTset_attribute_int(fileId, "/", "version", ver, 2);
		if( status < 0 ) {
			result = false;
		}
	}

	if( result ) {
		time_t t = time(0);
		struct tm *now = localtime(&t);
		char curTime[100];

		sprintf(curTime, "%2d-%2d-%4d, %2d:%2d",
	    		now->tm_mon + 1, now->tm_mday, now->tm_year + 1900,
	    		now->tm_hour, now->tm_min);

		status = H5LTset_attribute_string(fileId, "/", "creation date", curTime);
		if( status < 0 ) {
			result = false;
		}
	}
	return result;
}





int MData::GetSlideIdx(const char *slide)
{
	int		idx = 0;
	string	slideName = slide;

	while( idx < m_numSlides ) {
		if( slideName.compare(m_slides[idx]) == 0 )
			break;
		idx++;
	}
	return idx;
}








float *MData::GetSlideData(const string slide, int& numSlideObjs)
{
	float	*data = NULL;

	for(int i = 0; i < m_numSlides; i++) {
		if( slide.compare(m_slides[i]) == 0 ) {
			data = m_objects[m_dataIdx[i]];

			if( i + 1 < m_numSlides ) {
				numSlideObjs = m_dataIdx[i + 1] - m_dataIdx[i];
			} else {
				numSlideObjs = m_numObjs - m_dataIdx[i];
			}
			break;
		}
	}
	return data;
}






int MData::GetSlideOffset(const string slide, int& numSlideObjs)
{
	int		idx = -1;

	for(int i = 0; i < m_numSlides; i++) {
		if( slide.compare(m_slides[i]) == 0 ) {
			idx = m_dataIdx[i];

			if( i + 1 < m_numSlides ) {
				numSlideObjs = m_dataIdx[i + 1] - m_dataIdx[i];
			} else {
				numSlideObjs = m_numObjs - m_dataIdx[i];
			}
			break;
		}
	}

	return idx;
}
