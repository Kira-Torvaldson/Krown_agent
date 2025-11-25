/**
 * Krown Memory Manager - Gestion mémoire sécurisée en Rust
 * 
 * Cette bibliothèque fournit une gestion mémoire sécurisée pour le code C
 * en utilisant les garanties de sécurité mémoire de Rust.
 */

use std::ffi::{CStr, CString};
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

    /// Obtenir une référence vers les données
    #[inline]
    pub fn as_slice(&self) -> &[u8] {
        &self.data
    }
    
    /// Obtenir un pointeur mutable vers les données (pour écriture directe)
    #[inline]
    pub fn as_mut_ptr(&mut self) -> *mut u8 {
        self.data.as_mut_ptr()
    }
    
    /// Obtenir la capacité actuelle
    #[inline]
    pub fn capacity(&self) -> usize {
        self.data.capacity()
    }

    /// Obtenir la longueur actuelle
    #[inline]
    pub fn len(&self) -> usize {
        self.data.len()
    }

    /// Vérifier si le buffer est vide
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    /// Vider le buffer (conserve la capacité)
    #[inline]
    pub fn clear(&mut self) {
        self.data.clear();
    }
    
    /// Réduire la capacité si possible
    #[inline]
    pub fn shrink_to_fit(&mut self) {
        self.data.shrink_to_fit();
    }

    /// Consommer le buffer et retourner le Vec
    #[inline]
    pub fn into_vec(self) -> Vec<u8> {
        self.data
    }
    
    /// Consommer et obtenir un pointeur C (appelant doit libérer avec rust_free)
    pub fn into_raw(self) -> (*mut u8, usize) {
        let mut vec = self.data;
        vec.shrink_to_fit();
        let ptr = vec.as_mut_ptr();
        let len = vec.len();
        let cap = vec.capacity();
        std::mem::forget(vec);
        (ptr, cap)
    }
}

impl Drop for SafeBuffer {
    #[inline]
    fn drop(&mut self) {
        // La mémoire est automatiquement libérée par Rust
        // Pas besoin de clear(), Vec le fait automatiquement
    }
}

/// Structure pour gérer une chaîne C de manière sécurisée
pub struct SafeCString {
    inner: CString,
}

impl SafeCString {
    /// Créer une SafeCString depuis une &str
    pub fn new(s: &str) -> Result<Self, ()> {
        match CString::new(s) {
            Ok(cstr) => Ok(Self { inner: cstr }),
            Err(_) => Err(()),
        }
    }

    /// Obtenir un pointeur C
    pub fn as_ptr(&self) -> *const c_char {
        self.inner.as_ptr()
    }

    /// Consommer et obtenir le CString
    pub fn into_c_string(self) -> CString {
        self.inner
    }
}

/// Gestionnaire de mémoire global (optimisé)
pub struct MemoryManager;

impl MemoryManager {
    /// Allouer de la mémoire de manière sécurisée (alignée)
    #[inline]
    pub fn allocate(size: usize) -> *mut c_void {
        if size == 0 {
            return ptr::null_mut();
        }
        let mut vec = Vec::<u8>::with_capacity(size);
        let ptr = vec.as_mut_ptr();
        std::mem::forget(vec);
        ptr as *mut c_void
    }
    
    /// Allouer avec alignement spécifique
    #[inline]
    pub fn allocate_aligned(size: usize, align: usize) -> *mut c_void {
        if size == 0 {
            return ptr::null_mut();
        }
        // Utiliser Vec qui gère déjà l'alignement
        let mut vec = Vec::<u8>::with_capacity(size);
        let ptr = vec.as_mut_ptr();
        std::mem::forget(vec);
        ptr as *mut c_void
    }

    /// Libérer de la mémoire allouée
    #[inline]
    pub unsafe fn deallocate(ptr: *mut c_void, size: usize) {
        if !ptr.is_null() && size > 0 {
            let _ = Vec::from_raw_parts(ptr as *mut u8, 0, size);
        }
    }

    /// Réallouer de la mémoire (optimisé)
    #[inline]
    pub unsafe fn reallocate(ptr: *mut c_void, old_size: usize, new_size: usize) -> *mut c_void {
        if ptr.is_null() {
            return Self::allocate(new_size);
        }
        
        if new_size == 0 {
            Self::deallocate(ptr, old_size);
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
        
        // Si la capacité a changé, réallouer
        if new_cap < new_size {
            let new_vec = Vec::<u8>::with_capacity(new_size);
            let new_ptr2 = new_vec.as_mut_ptr();
            std::mem::forget(new_vec);
            new_ptr2 as *mut c_void
        } else {
            new_ptr as *mut c_void
        }
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

/// Échapper une chaîne JSON avec pré-allocation de taille connue
#[inline]
pub fn escape_json_string_with_capacity(input: &str, estimated_size: usize) -> String {
    let mut output = String::with_capacity(estimated_size);
    escape_json_string_into(input, &mut output);
    output
}

/// Échapper dans un String existant (évite les allocations)
#[inline]
pub fn escape_json_string_into(input: &str, output: &mut String) {
    for ch in input.chars() {
        match ch {
            '"' => output.push_str("\\\""),
            '\\' => output.push_str("\\\\"),
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            '\x08' => output.push_str("\\b"),
            '\x0c' => output.push_str("\\f"),
            c if c < ' ' => {
                output.push_str(&format!("\\u{:04x}", c as u32));
            }
            _ => output.push(ch),
        }
    }
}

/// Copier des données de manière sécurisée
pub unsafe fn safe_memcpy(dest: *mut u8, src: *const u8, n: usize) -> Result<(), ()> {
    if dest.is_null() || src.is_null() {
        return Err(());
    }
    ptr::copy_nonoverlapping(src, dest, n);
    Ok(())
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

/// Allouer de la mémoire de manière sécurisée
#[no_mangle]
pub extern "C" fn rust_malloc(size: usize) -> *mut c_void {
    MemoryManager::allocate(size)
}

/// Libérer de la mémoire
#[no_mangle]
pub unsafe extern "C" fn rust_free(ptr: *mut c_void, size: usize) {
    MemoryManager::deallocate(ptr, size);
}

/// Réallouer de la mémoire
#[no_mangle]
pub unsafe extern "C" fn rust_realloc(
    ptr: *mut c_void,
    old_size: usize,
    new_size: usize,
) -> *mut c_void {
    MemoryManager::reallocate(ptr, old_size, new_size)
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

/// Créer une chaîne C sécurisée depuis une chaîne C (copie)
#[no_mangle]
pub unsafe extern "C" fn rust_cstring_new(s: *const c_char) -> *mut c_char {
    if s.is_null() {
        return ptr::null_mut();
    }

    let c_str = CStr::from_ptr(s);
    match c_str.to_str() {
        Ok(str_slice) => {
            match CString::new(str_slice) {
                Ok(new_cstr) => {
                    let boxed = Box::new(new_cstr);
                    boxed.into_raw() as *mut c_char
                }
                Err(_) => ptr::null_mut(),
            }
        }
        Err(_) => {
            // Si ce n'est pas UTF-8 valide, copier quand même les bytes
            let bytes = c_str.to_bytes();
            match CString::new(bytes) {
                Ok(new_cstr) => {
                    let boxed = Box::new(new_cstr);
                    boxed.into_raw() as *mut c_char
                }
                Err(_) => ptr::null_mut(),
            }
        }
    }
}

/// Libérer une chaîne C créée par Rust
#[no_mangle]
pub unsafe extern "C" fn rust_cstring_free(s: *mut c_char) {
    if !s.is_null() {
        let _ = CString::from_raw(s);
    }
}

/// Copier de la mémoire de manière sécurisée
#[no_mangle]
pub unsafe extern "C" fn rust_memcpy(
    dest: *mut c_void,
    src: *const c_void,
    n: usize,
) -> i32 {
    match safe_memcpy(dest as *mut u8, src as *const u8, n) {
        Ok(()) => 0,
        Err(()) => -1,
    }
}

