use autocxx::prelude::*;
use std::ffi::{CString};
include_cpp! {
    #include "bindings.h"
    safety!(unsafe_ffi)
    generate!("HNSWBinding")
}

fn main() {
    
    let mut index = ffi::HNSWBinding::new().within_box();
    let space_dimenstion = CString::new("L2").expect("CString::new failed");
    unsafe {
        index.as_mut().createNewIndex(space_dimenstion.into_raw(), autocxx::c_int(5));
    }

    index.as_mut().initNewIndex(autocxx::c_int(100), autocxx::c_int(16), autocxx::c_int(200), autocxx::c_int(100)); 
    let mut vector1 = [3.1, 2.7, -1.0, 0.4, 0.6];
    let mut vector2 = [0.5, 1.7, 2.0, 1.2, 0.4];

    unsafe { 
        index.as_mut().addItem(vector1.as_mut_ptr(), true, autocxx::c_int(1));
        index.as_mut().addItem(vector2.as_mut_ptr(), true, autocxx::c_int(3));  
  
    };

    let mut indices:[autocxx::c_int;5] = [autocxx::c_int(0);5];
    let mut coefficients:[f32;5] = [0.0;5];
    unsafe { 
        let response = index.as_mut().knnQuery(vector2.as_mut_ptr(), true, autocxx::c_int(2),  indices.as_mut_ptr(), coefficients.as_mut_ptr());  
        println!("Search response: {}", i32::from(response));
        println!("Indices {:?}", indices);
        println!("Coefficients {:?}", coefficients);
    };


    println!("Total elements: {}", i32::from(index.getCurrentCount()));
    
}
