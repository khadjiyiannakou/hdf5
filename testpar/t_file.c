/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Parallel tests for file operations
 */

#include "testphdf5.h"

/*
 * test file access by communicator besides COMM_WORLD.
 * Split COMM_WORLD into two, one (even_comm) contains the original
 * processes of even ranks.  The other (odd_comm) contains the original
 * processes of odd ranks.  Processes in even_comm creates a file, then
 * cloose it, using even_comm.  Processes in old_comm just do a barrier
 * using odd_comm.  Then they all do a barrier using COMM_WORLD.
 * If the file creation and cloose does not do correct collective action
 * according to the communicator argument, the processes will freeze up
 * sooner or later due to barrier mixed up.
 */
void
test_split_comm_access(void)
{
    int mpi_size, mpi_rank;
    MPI_Comm comm;
    MPI_Info info = MPI_INFO_NULL;
    int is_old, mrc;
    int newrank, newprocs;
    hid_t fid;			/* file IDs */
    hid_t acc_tpl;		/* File access properties */
    herr_t ret;			/* generic return value */
    const char *filename;

    filename = (const char *)GetTestParameters();
    if (VERBOSE_MED)
	printf("Split Communicator access test on file %s\n",
	    filename);

    /* set up MPI parameters */
    MPI_Comm_size(MPI_COMM_WORLD,&mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD,&mpi_rank);
    is_old = mpi_rank%2;
    mrc = MPI_Comm_split(MPI_COMM_WORLD, is_old, mpi_rank, &comm);
    VRFY((mrc==MPI_SUCCESS), "");
    MPI_Comm_size(comm,&newprocs);
    MPI_Comm_rank(comm,&newrank);

    if (is_old){
	/* odd-rank processes */
	mrc = MPI_Barrier(comm);
	VRFY((mrc==MPI_SUCCESS), "");
    }else{
	/* even-rank processes */
	int sub_mpi_rank;	/* rank in the sub-comm */
	MPI_Comm_rank(comm,&sub_mpi_rank);

	/* setup file access template */
	acc_tpl = create_faccess_plist(comm, info, facc_type);
	VRFY((acc_tpl >= 0), "");

	/* create the file collectively */
	fid=H5Fcreate(filename,H5F_ACC_TRUNC,H5P_DEFAULT,acc_tpl);
	VRFY((fid >= 0), "H5Fcreate succeeded");

	/* Release file-access template */
	ret=H5Pclose(acc_tpl);
	VRFY((ret >= 0), "");

	/* close the file */
	ret=H5Fclose(fid);
	VRFY((ret >= 0), "");

	/* delete the test file */
	if (sub_mpi_rank == 0){
	    mrc = MPI_File_delete((char *)filename, info);
	    /*VRFY((mrc==MPI_SUCCESS), ""); */
	}
    }
    mrc = MPI_Comm_free(&comm);
    VRFY((mrc==MPI_SUCCESS), "MPI_Comm_free succeeded");
    mrc = MPI_Barrier(MPI_COMM_WORLD);
    VRFY((mrc==MPI_SUCCESS), "final MPI_Barrier succeeded");
}

void
test_file_properties(void)
{
    hid_t fid;                  /* HDF5 file ID */
    hid_t fapl_id;		/* File access plist */
    hbool_t is_coll;
    const char *filename;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Info info = MPI_INFO_NULL;
    int mpi_size, mpi_rank;
    herr_t ret;                 /* Generic return value */

    filename = (const char *)GetTestParameters();

    /* set up MPI parameters */
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    /* setup file access plist */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    VRFY((fapl_id >= 0), "H5Pcreate");
	ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret >= 0), "H5Pset_fapl_mpio");

    /* create the file */
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);
    VRFY((fid >= 0), "H5Fcreate succeeded");

    /* verify settings for file access properties */

    /* Collective metadata writes */
    ret = H5Pget_coll_metadata_write(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_coll_metadata_write succeeded");
    VRFY((is_coll == FALSE), "Incorrect property setting for coll metadata writes");

    /* Collective metadata read API calling requirement */
    ret = H5Pget_all_coll_metadata_ops(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_all_coll_metadata_ops succeeded");
    VRFY((is_coll == FALSE), "Incorrect property setting for coll metadata API calls requirement");

    ret = H5Fclose(fid);
    VRFY((ret >= 0), "H5Fclose succeeded");

    /* Open the file with the MPI-IO driver */
    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret >= 0), "H5Pset_fapl_mpio failed");
    fid = H5Fopen(filename, H5F_ACC_RDWR, fapl_id);
    VRFY((fid >= 0), "H5Fcreate succeeded");

    /* verify settings for file access properties */

    /* Collective metadata writes */
    ret = H5Pget_coll_metadata_write(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_coll_metadata_write succeeded");
    VRFY((is_coll == FALSE), "Incorrect property setting for coll metadata writes");

    /* Collective metadata read API calling requirement */
    ret = H5Pget_all_coll_metadata_ops(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_all_coll_metadata_ops succeeded");
    VRFY((is_coll == FALSE), "Incorrect property setting for coll metadata API calls requirement");

    ret = H5Fclose(fid);
    VRFY((ret >= 0), "H5Fclose succeeded");

    /* Open the file with the MPI-IO driver w collective settings */
    ret = H5Pset_fapl_mpio(fapl_id, comm, info);
    VRFY((ret >= 0), "H5Pset_fapl_mpio failed");
    /* Collective metadata writes */
    ret = H5Pset_coll_metadata_write(fapl_id, TRUE);
    VRFY((ret >= 0), "H5Pget_coll_metadata_write succeeded");
    /* Collective metadata read API calling requirement */
    ret = H5Pset_all_coll_metadata_ops(fapl_id, TRUE);
    VRFY((ret >= 0), "H5Pget_all_coll_metadata_ops succeeded");
    fid = H5Fopen(filename, H5F_ACC_RDWR, fapl_id);
    VRFY((fid >= 0), "H5Fcreate succeeded");

    /* verify settings for file access properties */

    /* Collective metadata writes */
    ret = H5Pget_coll_metadata_write(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_coll_metadata_write succeeded");
    VRFY((is_coll == TRUE), "Incorrect property setting for coll metadata writes");

    /* Collective metadata read API calling requirement */
    ret = H5Pget_all_coll_metadata_ops(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_all_coll_metadata_ops succeeded");
    VRFY((is_coll == TRUE), "Incorrect property setting for coll metadata API calls requirement");

    /* close fapl and retrieve it from file */
    ret = H5Pclose(fapl_id);
    VRFY((ret >= 0), "H5Pclose succeeded");
    fapl_id = -1;

    fapl_id = H5Fget_access_plist(fid);
    VRFY((fapl_id >= 0), "H5P_FILE_ACCESS");

    /* verify settings for file access properties */

    /* Collective metadata writes */
    ret = H5Pget_coll_metadata_write(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_coll_metadata_write succeeded");
    VRFY((is_coll == TRUE), "Incorrect property setting for coll metadata writes");

    /* Collective metadata read API calling requirement */
    ret = H5Pget_all_coll_metadata_ops(fapl_id, &is_coll);
    VRFY((ret >= 0), "H5Pget_all_coll_metadata_ops succeeded");
    VRFY((is_coll == TRUE), "Incorrect property setting for coll metadata API calls requirement");

    /* close file */
    ret = H5Fclose(fid);
    VRFY((ret >= 0), "H5Fclose succeeded");

    /* Release file-access plist */
    ret = H5Pclose(fapl_id);
    VRFY((ret >= 0), "H5Pclose succeeded");
} /* end test_file_properties() */

