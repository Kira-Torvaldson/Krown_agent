//! Krown Memory Manager - Gestion mémoire sécurisée en Rust

use std::ffi::CStr;
use std::os::raw::{c_char, c_void};
use std::ptr;
use std::slice;

/// Structure pour gérer un buffer de manière sécurisée (optimisée)
#[repr(C)]
pub struct SafeBuffer {
    data: Vec<u8>,
}

impl SafeBuffer {
    /// Créer un nouveau buffer avec une capacité initiale
    #[inline]
    pub fn new(initial_capacity: usize) -> Self {
        Self {
            data: Vec::with_capacity(initial_capacity.max(64)), // Minimum 64 bytes
        }
    }

    /// Ajouter des données au buffer (optimisé avec pré-allocation)
    #[inline]
    pub fn append(&mut self, data: &[u8]) -> Result<(), ()> {
        // Utiliser reserve_exact pour éviter les réallocations multiples
        let needed = self.data.len() + data.len();
        if needed > self.data.capacity() {
            // Croissance exponentielle : 1.5x au lieu de 2x pour économiser la mémoire
            let new_capacity = (self.data.capacity() * 3 / 2).max(needed);
            self.data.reserve_exact(new_capacity - self.data.len());
        }
        self.data.extend_from_slice(data);
        Ok(())
    }
    
    /// Ajouter des données sans vérification (plus rapide, à utiliser avec précaution)
    #[inline]
    pub unsafe fn append_unchecked(&mut self, data: &[u8]) {
        self.data.extend_from_slice(data);
    }

    #[inline]
    pub fn as_slice(&self) -> &[u8] {
        &self.data
    }

    #[inline]
    pub fn len(&self) -> usize {
        self.data.len()
    }
}

impl Drop for SafeBuffer {
    #[inline]
    fn drop(&mut self) {}
}


fn allocate(size: usize) -> *mut c_void {
    if size == 0 {
        return ptr::null_mut();
    }
    let mut vec = Vec::<u8>::with_capacity(size);
    let ptr = vec.as_mut_ptr();
    std::mem::forget(vec);
    ptr as *mut c_void
}

unsafe fn deallocate(ptr: *mut c_void, size: usize) {
    if !ptr.is_null() && size > 0 {
        let _ = Vec::from_raw_parts(ptr as *mut u8, 0, size);
    }
}

unsafe fn reallocate(ptr: *mut c_void, old_size: usize, new_size: usize) -> *mut c_void {
    if ptr.is_null() {
        return allocate(new_size);
    }
    if new_size == 0 {
        deallocate(ptr, old_size);
        return ptr::null_mut();
    }
    let mut old_vec = Vec::from_raw_parts(ptr as *mut u8, 0, old_size);
    if new_size > old_size {
        old_vec.reserve_exact(new_size - old_size);
    } else {
        old_vec.shrink_to_fit();
    }
    let new_ptr = old_vec.as_mut_ptr();
    let new_cap = old_vec.capacity();
    std::mem::forget(old_vec);
    if new_cap < new_size {
        let new_vec = Vec::<u8>::with_capacity(new_size);
        let new_ptr2 = new_vec.as_mut_ptr();
        std::mem::forget(new_vec);
        new_ptr2 as *mut c_void
    } else {
        new_ptr as *mut c_void
    }
}

/// Échapper une chaîne pour JSON de manière sécurisée (optimisé)
#[inline]
pub fn escape_json_string(input: &str) -> String {
    // Pré-allouer avec une estimation réaliste (la plupart des chaînes n'ont pas besoin d'échappement)
    let mut output = String::with_capacity(input.len() + input.len() / 8);
    let mut bytes = input.as_bytes();
    
    // Traitement optimisé byte-par-byte pour les caractères ASCII
    while let Some((&byte, rest)) = bytes.split_first() {
        match byte {
            b'"' => output.push_str("\\\""),
            b'\\' => output.push_str("\\\\"),
            b'\n' => output.push_str("\\n"),
            b'\r' => output.push_str("\\r"),
            b'\t' => output.push_str("\\t"),
            0x08 => output.push_str("\\b"),
            0x0c => output.push_str("\\f"),
            b if b < 0x20 => {
                output.push_str(&format!("\\u{:04x}", b as u32));
            }
            _ => {
                // Pour les bytes valides UTF-8, on peut les copier directement
                output.push(byte as char);
            }
        }
        bytes = rest;
    }
    output
}



// ============================================================================
// Interface FFI pour le code C
// ============================================================================

/// Allouer un buffer sécurisé (retourné comme pointeur opaque)
#[no_mangle]
pub extern "C" fn rust_buffer_new(initial_capacity: usize) -> *mut c_void {
    let buffer = Box::new(SafeBuffer::new(initial_capacity));
    Box::into_raw(buffer) as *mut c_void
}

/// Ajouter des données au buffer (optimisé)
#[no_mangle]
pub unsafe extern "C" fn rust_buffer_append(
    buffer_ptr: *mut c_void,
    data: *const u8,
    data_len: usize,
) -> i32 {
    if buffer_ptr.is_null() || data.is_null() || data_len == 0 {
        return if data_len == 0 { 0 } else { -1 };
    }

    let buffer = &mut *(buffer_ptr as *mut SafeBuffer);
    let slice = slice::from_raw_parts(data, data_len);
    
    // Utiliser append_unchecked pour les performances si on est sûr de la capacité
    if buffer.data.len() + data_len <= buffer.data.capacity() {
        buffer.append_unchecked(slice);
        0
    } else {
        match buffer.append(slice) {
            Ok(()) => 0,
            Err(()) => -1,
        }
    }
}

/// Obtenir la longueur du buffer
#[no_mangle]
pub unsafe extern "C" fn rust_buffer_len(buffer_ptr: *const c_void) -> usize {
    if buffer_ptr.is_null() {
        return 0;
    }
    let buffer = &*(buffer_ptr as *const SafeBuffer);
    buffer.len()
}

/// Obtenir un pointeur vers les données du buffer (read-only)
#[no_mangle]
pub unsafe extern "C" fn rust_buffer_data(buffer_ptr: *const c_void) -> *const u8 {
    if buffer_ptr.is_null() {
        return ptr::null();
    }
    let buffer = &*(buffer_ptr as *const SafeBuffer);
    buffer.as_slice().as_ptr()
}

/// Libérer un buffer
#[no_mangle]
pub unsafe extern "C" fn rust_buffer_free(buffer_ptr: *mut c_void) {
    if !buffer_ptr.is_null() {
        let _ = Box::from_raw(buffer_ptr as *mut SafeBuffer);
    }
}

#[no_mangle]
pub extern "C" fn rust_malloc(size: usize) -> *mut c_void {
    allocate(size)
}

#[no_mangle]
pub unsafe extern "C" fn rust_free(ptr: *mut c_void, size: usize) {
    deallocate(ptr, size);
}

#[no_mangle]
pub unsafe extern "C" fn rust_realloc(
    ptr: *mut c_void,
    old_size: usize,
    new_size: usize,
) -> *mut c_void {
    reallocate(ptr, old_size, new_size)
}

/// Échapper une chaîne pour JSON (optimisé)
#[no_mangle]
pub unsafe extern "C" fn rust_escape_json(
    input: *const c_char,
    output: *mut c_char,
    output_size: usize,
) -> i32 {
    if input.is_null() || output.is_null() || output_size == 0 {
        return -1;
    }

    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return -1,
    };

    // Vérifier d'abord si l'échappement est nécessaire (optimisation)
    let needs_escape = c_str.bytes().any(|b| matches!(b, b'"' | b'\\' | b'\n' | b'\r' | b'\t' | 0x08 | 0x0c) || b < 0x20);
    
    if !needs_escape {
        // Pas besoin d'échappement, copier directement
        if c_str.len() >= output_size {
            return -1;
        }
        ptr::copy_nonoverlapping(c_str.as_ptr(), output, c_str.len());
        *output.add(c_str.len()) = 0;
        return c_str.len() as i32;
    }

    let escaped = escape_json_string(c_str);
    let escaped_bytes = escaped.as_bytes();

    if escaped_bytes.len() >= output_size {
        return -1; // Buffer trop petit
    }

    ptr::copy_nonoverlapping(escaped_bytes.as_ptr(), output as *mut u8, escaped_bytes.len());
    *output.add(escaped_bytes.len()) = 0; // Null terminator
    escaped_bytes.len() as i32
}

#[no_mangle]
pub unsafe extern "C" fn rust_memcpy(
    dest: *mut c_void,
    src: *const c_void,
    n: usize,
) -> i32 {
    if dest.is_null() || src.is_null() {
        return -1;
    }
    ptr::copy_nonoverlapping(src as *const u8, dest as *mut u8, n);
    0
}

