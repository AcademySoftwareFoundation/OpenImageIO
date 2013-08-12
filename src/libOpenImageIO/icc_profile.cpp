#include <ctype.h>
#include <cstdio>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

#include <boost/foreach.hpp>

#include "fmath.h"

#include "imageio.h"
OIIO_NAMESPACE_ENTER
{

#define ICC_HEADER_SIZE 14

bool read_jpeg_icc_profile(unsigned char* icc_data, unsigned int icc_datasize, ImageSpec &spec){
	int num_markers = 0;
	int seq_no;
	unsigned char* icc_buf=NULL;
	int total_length=0;

	const int MAX_SEQ_NO = 255;			// sufficient since marker numbers are bytes
	unsigned char marker_present[MAX_SEQ_NO+1];	// 1 if marker found
	unsigned data_length[MAX_SEQ_NO+1];	// size of profile data in marker
	unsigned data_offset[MAX_SEQ_NO+1];	// offset for data in marker
	
	memset(marker_present,0,(MAX_SEQ_NO+1));
	num_markers=icc_data[13];
	seq_no=icc_data[12];
	if(seq_no<=0&&seq_no>num_markers){
		return false;
	}

	data_length[seq_no]=icc_datasize - ICC_HEADER_SIZE;
	for(seq_no=1;seq_no <= num_markers; seq_no++){

		marker_present[seq_no]=1;

		data_offset[seq_no]=total_length;
		total_length += data_length[seq_no];
	}

	if(total_length <=0) return false; // found only empty markers
	
	icc_buf = (unsigned char* )malloc(total_length*sizeof(unsigned char));
	if (icc_buf==NULL)
		return false;	// out of memory
	
	seq_no = icc_data[12];
	unsigned char* dst_ptr=icc_buf+data_offset[seq_no];
	unsigned char* src_ptr=icc_data+ICC_HEADER_SIZE;
	int length=data_length[seq_no];
	while(length--){
		*dst_ptr++=*src_ptr++;
	}
	
	spec.set_icc_profile(icc_buf,total_length);

	return true;
};

bool create_icc_profile(unsigned char* icc_data, unsigned int icc_datasize, ImageSpec &spec){
	unsigned char* icc_buf=NULL;
	int total_length=0;
	icc_buf = (unsigned char* )malloc(icc_datasize*sizeof(unsigned char));
	if(icc_buf){
		memcpy(icc_buf,icc_data,icc_datasize);
		total_length=icc_datasize;
	}
	spec.set_icc_profile(icc_buf, total_length);
	return true;
};

}
OIIO_NAMESPACE_EXIT