
/*
  This file is part of MADNESS.

  Copyright (C) 2007,2010 Oak Ridge National Laboratory

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

  For more information please contact:

  Robert J. Harrison
  Oak Ridge National Laboratory
  One Bethel Valley Road
  P.O. Box 2008, MS-6367

  email: harrisonrj@ornl.gov
  tel:   865-241-3937
  fax:   865-572-0680

  $Id$
*/
/*!
  \file helium_mp2.cc
  \brief Solves the Hartree-Fock and MP2 equations for the helium atom
  \defgroup examplehehf Hartree-Fock and MP2 for the helium atom
  \ingroup examples

  The source is
  <a href=http://code.google.com/p/m-a-d-n-e-s-s/source/browse/local/trunk/src/apps/examples/helium_mp2.cc>here</a>.


*/


//#define WORLD_INSTANTIATE_STATIC_TEMPLATES
#include <madness/mra/mra.h>
#include <madness/mra/operator.h>
#include <madness/mra/funcplot.h>
#include <madness/mra/lbdeux.h>

#include <iostream>

using namespace madness;

struct tensortxt {
	void operator()(const Key<6>& key, const GenTensor<double>& t) const {
		int i=0;
//		if (key.level()==5) {
		if (t.has_data()) {
			std::stringstream ss;
			ss << key;
			FILE * pFile = fopen (ss.str().c_str(),"w");
			int k=t.dim(0);
			Tensor<double> tt=t.full_tensor().reshape(k*k*k, k*k*k);
			for (int i=0; i<k*k*k; ++i) {
				for (int j=0; j<k*k*k; ++j) {
					fprintf(pFile,"%16.12f",tt(i,j));
				}
				fprintf(pFile,"\n");
			}
			fclose(pFile);
		}
	}
};

namespace madness{
extern std::vector<std::string> cubefile_header(std::string filename="input", const bool& no_orient=false);
}
template<size_t NDIM>
void load_function(World& world, Function<double,NDIM>& pair, const std::string name) {
    if (world.rank()==0)  print("loading function ", name);

    archive::ParallelInputArchive ar(world, name.c_str());
    ar & pair;

    FunctionDefaults<3>::set_k(pair.k());
    FunctionDefaults<6>::set_k(pair.k());

    FunctionDefaults<3>::set_thresh(pair.thresh());
    FunctionDefaults<6>::set_thresh(pair.thresh());

    std::string line="loaded function "+name;
    pair.print_size(line);

}
template<size_t NDIM>
void save_function(World& world, Function<double,NDIM>& pair, const std::string name) {
    if (world.rank()==0)  print("loading function ", name);

    archive::ParallelOutputArchive ar(world, name.c_str());
    ar & pair;

    std::string line="saved function "+name;
    pair.print_size(line);

}


template<size_t NDIM>
void draw_line(World& world, Function<double,NDIM>& pair, const std::string restart_name) {

    Vector<double,NDIM> lo(0.0), hi(0.0);
    lo[2]=-8.0;
    hi[2]=8.0;

    {
        std::string filename="line_"+restart_name;
        trajectory<NDIM> line=trajectory<NDIM>::line2(lo,hi,601);
        plot_along<NDIM>(world,line,pair,filename);
    }

}

template<size_t NDIM>
void draw_circle(World& world, Function<double,NDIM>& pair, const std::string restart_name) {

	std::string filename="circle_"+restart_name;
	coord_3d el2(0.0);
	el2[1]=0.5;
	trajectory<NDIM> circ(0.5,el2,601);
	plot_along<NDIM>(world,circ,pair,filename);

}


void dostuff(World& world) {

	real_function_6d Uphi0=real_factory_6d(world);
	load_function(world,Uphi0,"result_before_reconstruction");
    FunctionDefaults<6>::set_tensor_type(TT_FULL);
	Uphi0.get_impl()->set_tensor_args(TensorArgs(TT_FULL,1.e-3));
    Uphi0.change_tensor_type(TensorArgs(TT_FULL,1.e-3));


	Uphi0.print_size("result before reconstruction");
	Uphi0.reconstruct();
	Uphi0.print_size("result after reconstruction");
	Uphi0.compress();
	Uphi0.print_size("result after compression");
	Uphi0.get_impl()->timer_filter.print("filter");
	Uphi0.get_impl()->timer_compress_svd.print("compress_svd");

    FunctionDefaults<6>::set_tensor_type(TT_2D);
	Uphi0.get_impl()->set_tensor_args(TensorArgs(TT_2D,1.e-3));
	Uphi0.change_tensor_type(TensorArgs(TT_2D,1.e-3));
	Uphi0.print_size("result in TT_2D");
	Uphi0.get_impl()->print_stats();

	throw;

}


int main(int argc, char** argv) {
    initialize(argc, argv);
    World world(SafeMPI::COMM_WORLD);
    startup(world,argc,argv);
    std::cout.precision(6);


    // determine the box size L
    double L=-1.0;
    bool no_orient=false;
    std::ifstream f("input");
    position_stream(f, "dft");
    std::string s;
    while (f >> s) {
    	if (s == "end") {
    		break;
    	} else if (s == "L") {
    		f >> L;
        } else if (s == "no_orient") {
                no_orient=true;
        }
    }
    if (L<0.0) MADNESS_EXCEPTION("box size indetermined",1);
    FunctionDefaults<3>::set_cubic_cell(-L,L);
    FunctionDefaults<6>::set_cubic_cell(-L,L);
    FunctionDefaults<6>::set_tensor_type(TT_2D);
//    FunctionDefaults<6>::set_tensor_type(TT_FULL);


    if (world.rank()==0) {
     	    print("cell size:         ", FunctionDefaults<6>::get_cell_width()[0]);
    }

    // load the function of interest
    std::vector<std::string> filenames;

    for(int i = 1; i < argc; i++) {
        const std::string arg=argv[i];

        // break parameters into key and val
        size_t pos=arg.find("=");
        std::string key=arg.substr(0,pos);
        std::string val=arg.substr(pos+1);

        if (key=="file") {                               // usage: restart=path/to/mo_file
            filenames.push_back(stringify(val));
        }
    }
    FunctionDefaults<6>::set_thresh(1.e-3);
	// make sure we're doing what we want to do
	if (world.rank()==0) {
		print("polynomial order:  ", FunctionDefaults<6>::get_k());
		print("threshold (6D):    ", FunctionDefaults<6>::get_thresh());
		print("cell size:         ", FunctionDefaults<6>::get_cell()(0,1) - FunctionDefaults<6>::get_cell()(0,0));
		print("truncation mode:   ", FunctionDefaults<6>::get_truncate_mode());
		print("tensor type:       ", FunctionDefaults<6>::get_tensor_type());
		print("");
		print("facReduce          ", GenTensor<double>::fac_reduce());
		print("max displacement   ", Displacements<6>::bmax_default());
		print("apply randomize    ", FunctionDefaults<6>::get_apply_randomize());
		print("world.size()       ", world.size());
		print("no_orient          ", no_orient);
		print("");
	}

	dostuff(world);

    try {
        static const size_t NDIM=3;
        std::vector<Function<double,NDIM> > vf;
        for (size_t i=0; i<filenames.size(); ++i) {
            real_function_3d tmp;
            try { // load a single function
                load_function(world,tmp,filenames[i]);
                vf.push_back(tmp);
            } catch (...) { // load a vector of functions
                std::vector<Function<double,NDIM> > tmp2;
                load_function(world,tmp2,filenames[i]);
                for (auto& t : tmp2) vf.push_back(t);
            }
        }
		plot_plane(world,vf,filenames[0]);

		double width = FunctionDefaults<3>::get_cell_min_width()/2.0 - 1.e-3;
		coord_3d start(0.0); start[0]=-width;
		coord_3d end(0.0); end[0]=width;
		plot_line(("line_"+filenames[0]).c_str(),10000,start,end,vf[0]);

		// plot the Gaussian cube file
		std::vector<std::string> molecular_info=cubefile_header("input",no_orient);
		std::string filename=filenames[0]+".cube";
		plot_cubefile<3>(world,vf[0],filename,molecular_info);

    } catch (...) {
        try {
            static const size_t NDIM=6;
            std::vector<Function<double,NDIM> > vf(filenames.size());
            for (size_t i=0; i<filenames.size(); ++i) load_function(world,vf[i],filenames[i]);
            plot_plane(world,vf,filenames[0]);
        } catch (...) {

        }
    }



    world.gop.fence();
    print("exiting tiny");

    return 0;
}

