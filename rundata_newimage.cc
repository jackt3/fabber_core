/**
 * fabber_rundata_newimage.cc
 *
 * Martin Craig
 *
 * Copyright (C) 2016 University of Oxford  */

/*  CCOPYRIGHT */

#include "rundata_newimage.h"

#include "easylog.h"
#include "rundata.h"

#include <newimage/newimage.h>
#include <newimage/newimageio.h>
#include <newmat.h>

#include <ostream>
#include <string>
#include <vector>

using namespace std;
using namespace NEWIMAGE;
using NEWMAT::Matrix;

static void DumpVolumeInfo(const volume4D<float>& info, ostream& out)
{
    out << "FabberRunDataNewimage::Dimensions: x=" << info.xsize() << ", y=" << info.ysize() << ", z=" << info.zsize()
        << ", vols=" << info.tsize() << endl;
    out << "FabberRunDataNewimage::Voxel size: x=" << info.xdim() << "mm, y=" << info.ydim() << "mm, z=" << info.zdim()
        << "mm, TR=" << info.tdim() << " sec\n";
    out << "FabberRunDataNewimage::Intents: " << info.intent_code() << ", " << info.intent_param(1) << ", "
        << info.intent_param(2) << ", " << info.intent_param(3) << endl;
}

static void DumpVolumeInfo(const volume<float>& info, ostream& out)
{
    out << "FabberRunDataNewimage::Dimensions: x=" << info.xsize() << ", y=" << info.ysize() << ", z=" << info.zsize()
        << ", vols=1" << endl;
    out << "FabberRunDataNewimage::Voxel size: x=" << info.xdim() << "mm, y=" << info.ydim() << "mm, z=" << info.zdim()
        << "mm, TR=1"
        << " sec\n";
    out << "FabberRunDataNewimage::Intents: " << info.intent_code() << ", " << info.intent_param(1) << ", "
        << info.intent_param(2) << ", " << info.intent_param(3) << endl;
}

FabberRunDataNewimage::FabberRunDataNewimage(bool compat_options)
    : FabberRunData(compat_options)
    , m_have_mask(false)
{
}

void FabberRunDataNewimage::SetExtentFromData()
{
    string mask_fname = GetStringDefault("mask", "");
    m_have_mask = (mask_fname != "");

    if (m_have_mask) {
        LOG << "FabberRunDataNewimage::Loading mask data from '" + mask_fname << "'" << endl;
        read_volume(m_mask, mask_fname);
        m_mask.binarise(1e-16, m_mask.max() + 1, exclusive);
        DumpVolumeInfo(m_mask, LOG);
        SetCoordsFromExtent(m_mask.xsize(), m_mask.ysize(), m_mask.zsize());
    } else {
        // Make sure the coords are loaded from the main data even if we don't
        // have a mask, and that the reference volume is initialized
        string data_fname = GetStringDefault("data", GetStringDefault("data1", ""));
        volume<float> main_vol;
        read_volume(main_vol, data_fname);
        SetCoordsFromExtent(main_vol.xsize(), main_vol.ysize(), main_vol.zsize());
    }
}

const Matrix& FabberRunDataNewimage::GetVoxelData(const std::string& key)
{
    // Attempt to load data if not already present. Will
    // throw an exception if parameter not specified
    // or file could not be loaded
    //
    // FIXME different exceptions? What about use case where
    // data is optional?
    string key_cur = key;
    string filename = "";
    while (key_cur != "") {
        filename = key_cur;
        key_cur = GetStringDefault(key_cur, "");
        // Avoid possible circular reference!
        if (key_cur == key)
            break;
    }

    LOG << "FabberRunDataNewimage::Looking for " << key << " in " << filename << endl;
    if (m_voxel_data.find(filename) == m_voxel_data.end()) {
        LOG << "FabberRunDataNewimage::Loading data from '" + filename << "'" << endl;
        // Load the data file using Newimage library
        // FIXME should check for presence of file before trying to load.
        if (!fsl_imageexists(filename)) {
            throw DataNotFound(filename, "File is invalid or does not exist");
        }

        volume4D<float> vol;
        try {
            read_volume4D(vol, filename);
            if (!m_have_mask) {
                // We need a mask volume so that when we save we can make sure
                // the image properties are set consistently with the source data
                m_mask = vol[0];
                m_mask = 1;
                m_have_mask = true;
            }
        } catch (...) {
            throw DataNotFound(filename, "Error loading file");
        }
        DumpVolumeInfo(vol, LOG);

        try {
            if (m_have_mask) {
                LOG << "FabberRunDataNewimage::Applying mask to data..." << endl;
                m_voxel_data[filename] = vol.matrix(m_mask);
            } else {
                m_voxel_data[filename] = vol.matrix();
            }
        } catch (exception& e) {
            LOG << "NEWMAT error while applying mask... Most likely a dimension mismatch. ***\n";
            throw;
        }
    }

    return FabberRunData::GetVoxelData(key);
}

void FabberRunDataNewimage::SaveVoxelData(const std::string& filename, NEWMAT::Matrix& data, VoxelDataType data_type)
{
    LOG << "FabberRunDataNewimage::Saving to nifti: " << filename << endl;
    int nifti_intent_code;
    switch (data_type) {
    case VDT_MVN:
        nifti_intent_code = NIFTI_INTENT_SYMMATRIX;
        break;
    default:
        nifti_intent_code = NIFTI_INTENT_NONE;
    }

    int data_size = data.Nrows();
    volume4D<float> output(m_extent[0], m_extent[1], m_extent[2], data_size);
    if (m_have_mask) {
        output.setmatrix(data, m_mask);
    } else {
        output.setmatrix(data);
    }

    output.set_intent(nifti_intent_code, 0, 0, 0);
    output.setDisplayMaximumMinimum(output.max(), output.min());

    string filepath = GetOutputDir() + "/" + filename;
    save_volume4D(output, filepath);
}

void FabberRunDataNewimage::SetCoordsFromExtent(int nx, int ny, int nz)
{
    LOG << "FabberRunDataNewimage::Setting coordinates from extent" << endl;

    FabberRunData::SetExtent(nx, ny, nz);

    volume4D<float> coordvol(nx, ny, nz, 3);
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            for (int k = 0; k < nz; k++) {
                ColumnVector vcoord(3);
                vcoord << i << j << k;
                coordvol.setvoxelts(vcoord, i, j, k);
            }
        }
    }

    if (m_have_mask) {
        SetVoxelCoords(coordvol.matrix(m_mask));
    } else {
        SetVoxelCoords(coordvol.matrix());
    }
}