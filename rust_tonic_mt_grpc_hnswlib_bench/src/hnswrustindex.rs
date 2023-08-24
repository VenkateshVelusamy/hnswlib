use autocxx::prelude::*;
use std::pin::Pin;
use rand::{thread_rng, Rng};
use std::convert::TryInto;
use std::ffi::{CString};
use crate::hnswrustindex::ffi::HNSWBinding;

include_cpp! {
    #include "bindings.h"
    safety!(unsafe_ffi)
    generate!("HNSWBinding")
}

pub struct Index{
    index: Pin<Box<HNSWBinding>>
}

impl Index {
    pub fn new() -> Self {
        // Initializing an index with 128 dimesnions
        let n = 10000;
        const dimension:usize = 128;
        let ffiIndex = ffi::HNSWBinding::new(autocxx::c_int(dimension as i32)).within_box();
        ffiIndex.initNewIndex(autocxx::c_int(n as i32), autocxx::c_int(100), autocxx::c_int(1024), autocxx::c_int(100));
        
        // create sample vectors
        let mut sample_vectors: Vec<_> = Vec::with_capacity(n);
        for _i in 0..n {
            let mut sample_vector = Vec::with_capacity(dimension);
            for _j in 0..dimension {
                sample_vector.push(rand::thread_rng().gen());
                
            }
            let mut sample_vector_arr: [f32; dimension] = sample_vector.try_into().unwrap();
            sample_vectors.push(sample_vector_arr);
        }
        // create a queryable vector
        let mut queryable_vector = [0.051;128];
        let mut queryable_vector2 = [0.054;128];


        unsafe { 
            // replace the first element with the queryable vector1
            ffiIndex.addItem(queryable_vector.as_mut_ptr(), true, autocxx::c_int(0 as i32));
           
            for _i in 1..n-1 {
                ffiIndex.addItem(sample_vectors[_i].as_mut_ptr(), true, autocxx::c_int(_i as i32));
            }

            // replace the last element with the queryable vector2
            ffiIndex.addItem(queryable_vector2.as_mut_ptr(), true, autocxx::c_int((n-1) as i32));
            // set higher search EF
            ffiIndex.setEf(autocxx::c_int(10000 as i32));
        }

        return Index {
            index: ffiIndex
        };
    }

    pub fn searchIndex(mut self) {
        const dimension:usize = 128;
        let mut query_vector: [f32; dimension] = [0.051; dimension];

        const k:usize = 2;
        let mut indices:[autocxx::c_int;k] = [autocxx::c_int(0);k];
        let mut coefficients:[f32;k] = [0.0;k];

        unsafe { 
            let response = self.index.as_mut().knnQuery(query_vector.as_mut_ptr(), true, autocxx::c_int(k as i32),  indices.as_mut_ptr(), coefficients.as_mut_ptr());
            println!("Indices {:?}", indices);
            println!("Coefficients {:?}", coefficients);
            println!("Total elements: {}", i32::from(self.index.getCurrentCount()));
        };
    }
}

