// Copyright 2025 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tensorflow/lite/experimental/litert/runtime/gl_buffer.h"

#include <stdlib.h>

#include <cstddef>
#include <memory>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "tensorflow/lite/experimental/litert/c/litert_common.h"
#include "tensorflow/lite/experimental/litert/c/litert_gl_types.h"
#include "tensorflow/lite/experimental/litert/c/litert_logging.h"
#include "tensorflow/lite/experimental/litert/c/litert_tensor_buffer.h"
#include "tensorflow/lite/experimental/litert/cc/litert_expected.h"
#include "tensorflow/lite/experimental/litert/cc/litert_macros.h"

#if LITERT_HAS_OPENGL_SUPPORT
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>

#include "tensorflow/lite/delegates/gpu/gl/gl_buffer.h"
#endif  // LITERT_HAS_OPENGL_SUPPORT

namespace litert {
namespace internal {

#if LITERT_HAS_AHWB_SUPPORT

PFNGLBUFFERSTORAGEEXTERNALEXTPROC glBufferStorageExternalEXT;
PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC eglGetNativeClientBufferANDROID;
PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;

bool IsAhwbToGlInteropSupported() {
  static const bool extensions_allowed = [] {
    eglGetNativeClientBufferANDROID =
        reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(
            eglGetProcAddress("eglGetNativeClientBufferANDROID"));
    glBufferStorageExternalEXT =
        reinterpret_cast<PFNGLBUFFERSTORAGEEXTERNALEXTPROC>(
            eglGetProcAddress("glBufferStorageExternalEXT"));
    eglDupNativeFenceFDANDROID =
        reinterpret_cast<PFNEGLDUPNATIVEFENCEFDANDROIDPROC>(
            eglGetProcAddress("eglDupNativeFenceFDANDROID"));
    eglCreateSyncKHR = reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(
        eglGetProcAddress("eglCreateSyncKHR"));
    eglWaitSyncKHR = reinterpret_cast<PFNEGLWAITSYNCKHRPROC>(
        eglGetProcAddress("eglWaitSyncKHR"));
    eglClientWaitSyncKHR = reinterpret_cast<PFNEGLCLIENTWAITSYNCKHRPROC>(
        eglGetProcAddress("eglClientWaitSyncKHR"));
    eglDestroySyncKHR = reinterpret_cast<PFNEGLDESTROYSYNCKHRPROC>(
        eglGetProcAddress("eglDestroySyncKHR"));
    return eglClientWaitSyncKHR && eglWaitSyncKHR &&
           eglGetNativeClientBufferANDROID && glBufferStorageExternalEXT &&
           eglCreateSyncKHR && eglDupNativeFenceFDANDROID && eglDestroySyncKHR;
  }();
  return extensions_allowed;
}

Expected<GlBuffer> GlBuffer::AllocFromAhwbBuffer(AhwbBuffer& ahwb_buffer) {
  LITERT_RETURN_IF_ERROR(
      IsAhwbToGlInteropSupported(),
      Unexpected(kLiteRtStatusErrorRuntimeFailure,
                 "AHardwareBuffer to GL interop is not supported"));
  LITERT_RETURN_IF_ERROR(
      ahwb_buffer.ahwb != nullptr,
      Unexpected(kLiteRtStatusErrorRuntimeFailure, "AHardwareBuffer is null"));

  // Create GL buffer id.
  GLuint gl_id;
  glGenBuffers(1, &gl_id);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl_id);

  // Create EGLClientBuffer from AHardwareBuffer.
  EGLClientBuffer native_buffer =
      eglGetNativeClientBufferANDROID(ahwb_buffer.ahwb);
  LITERT_RETURN_IF_ERROR(
      native_buffer != nullptr,
      Unexpected(kLiteRtStatusErrorRuntimeFailure,
                 "Failed to create EGLClientBuffer from AHardwareBuffer"));

  LITERT_ASSIGN_OR_RETURN(
      size_t size_bytes,
      litert::internal::AhwbBuffer::GetSize(ahwb_buffer.ahwb));
  LITERT_RETURN_IF_ERROR(size_bytes != 0,
                         Unexpected(kLiteRtStatusErrorRuntimeFailure,
                                    "AHardwareBuffer size is 0"));

  // Create OpenGl buffer object backed by the AHardwareBuffer.
  glBufferStorageExternalEXT(
      GL_SHADER_STORAGE_BUFFER, 0, size_bytes, native_buffer,
      GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT_EXT |
          GL_MAP_PERSISTENT_BIT_EXT);
  // Check for OpenGL errors.
  absl::Status status = tflite::gpu::gl::GetOpenGlErrors();
  if (!status.ok()) {
    return Unexpected(kLiteRtStatusErrorRuntimeFailure,
                      absl::StrCat("glBufferStorageExternalEXT: Failed to "
                                   "create GL buffer from AHardwareBuffer: ",
                                   status.message()));
  }
  // Unbind the buffer.
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Create GL buffer object. We assume ownership of the GL buffer id so that it
  // will be automatically deallocated when the internal::GlBuffer is destroyed.
  tflite::gpu::gl::GlBuffer tflite_gl_buffer(GL_SHADER_STORAGE_BUFFER, gl_id,
                                             size_bytes, /*offset=*/0,
                                             /*has_ownership=*/true);
  return GlBuffer(std::move(tflite_gl_buffer), ahwb_buffer.ahwb);
}
#endif  // LITERT_HAS_AHWB_SUPPORT

GlBuffer::GlBuffer(LiteRtGLenum target, LiteRtGLuint id, size_t size_bytes,
                   size_t offset, LiteRtGlBufferDeallocator deallocator) {
#if LITERT_HAS_OPENGL_SUPPORT
  size_bytes_ = size_bytes;

  if (deallocator != nullptr) {
    tflite_gl_buffer_ = tflite::gpu::gl::GlBuffer(
        target, id, size_bytes, offset, /*has_ownership=*/false);
    deallocator_ = std::move(deallocator);
  } else {
    tflite_gl_buffer_ = tflite::gpu::gl::GlBuffer(
        target, id, size_bytes, offset, /*has_ownership=*/true);
    deallocator_ = nullptr;
  }
#else
  LITERT_LOG(LITERT_ERROR, "GlBuffer::GlBuffer() is not supported");
#endif  // LITERT_HAS_OPENGL_SUPPORT
}

GlBuffer::GlBuffer(GlBuffer&& other) {
#if LITERT_HAS_OPENGL_SUPPORT
  tflite_gl_buffer_ = std::move(other.tflite_gl_buffer_);
  deallocator_ = std::move(other.deallocator_);
  data_ = other.data_;
  size_bytes_ = other.size_bytes_;
#if LITERT_HAS_AHWB_SUPPORT
  ahwb_ = other.ahwb_;
#endif  // LITERT_HAS_AHWB_SUPPORT
  // Reset the other GlBuffer to a default state.
  other.data_ = nullptr;
  other.size_bytes_ = 0;
#if LITERT_HAS_AHWB_SUPPORT
  other.ahwb_ = nullptr;
#endif  // LITERT_HAS_AHWB_SUPPORT
#else
  LITERT_LOG(LITERT_ERROR, "GlBuffer::GlBuffer() is not supported");
#endif  // LITERT_HAS_OPENGL_SUPPORT
}

GlBuffer::~GlBuffer() {
#if LITERT_HAS_OPENGL_SUPPORT
  if (deallocator_ != nullptr) {
    deallocator_(reinterpret_cast<void*>(tflite_gl_buffer_.id()));
  }
  if (data_ != nullptr) {
    free(data_);
  }
#else
  LITERT_LOG(LITERT_ERROR, "GlBuffer::~GlBuffer() is not supported");
#endif  // LITERT_HAS_OPENGL_SUPPORT
}

LiteRtGLenum GlBuffer::target() const {
#if LITERT_HAS_OPENGL_SUPPORT
  return tflite_gl_buffer_.target();
#else
  LITERT_LOG(LITERT_ERROR, "GlBuffer::target() is not supported");
  return 0;
#endif  // LITERT_HAS_OPENGL_SUPPORT
}
LiteRtGLuint GlBuffer::id() const {
#if LITERT_HAS_OPENGL_SUPPORT
  return tflite_gl_buffer_.id();
#else
  LITERT_LOG(LITERT_ERROR, "GlBuffer::id() is not supported");
  return 0;
#endif  // LITERT_HAS_OPENGL_SUPPORT
}
size_t GlBuffer::size_bytes() const {
#if LITERT_HAS_OPENGL_SUPPORT
  return tflite_gl_buffer_.bytes_size();
#else
  LITERT_LOG(LITERT_ERROR, "GlBuffer::size_bytes() is not supported");
  return 0;
#endif  // LITERT_HAS_OPENGL_SUPPORT
}
size_t GlBuffer::offset() const {
#if LITERT_HAS_OPENGL_SUPPORT
  return tflite_gl_buffer_.offset();
#else
  LITERT_LOG(LITERT_ERROR, "GlBuffer::offset() is not supported");
  return 0;
#endif
}

Expected<GlBuffer> GlBuffer::Alloc(size_t size_bytes) {
#if LITERT_HAS_OPENGL_SUPPORT
  tflite::gpu::gl::GlBuffer tflite_gl_buffer;

  if (!tflite::gpu::gl::CreateReadWriteShaderStorageBuffer<std::byte>(
           size_bytes, &tflite_gl_buffer)
           .ok()) {
    return Unexpected(kLiteRtStatusErrorRuntimeFailure,
                      "Failed to allocate GL buffer");
  }

  return GlBuffer(std::move(tflite_gl_buffer));
#else
  return Unexpected(kLiteRtStatusErrorRuntimeFailure,
                    "OpenGL buffers are not supported");
#endif  // LITERT_HAS_OPENGL_SUPPORT
}

template Expected<float*> GlBuffer::Lock<float>();
template Expected<char*> GlBuffer::Lock<char>();
template Expected<void> GlBuffer::Unlock<float>();
template Expected<void> GlBuffer::Unlock<char>();

template <typename T>
Expected<T*> GlBuffer::Lock() {
#if LITERT_HAS_OPENGL_SUPPORT
  absl::MutexLock lock(&mutex_);
#if LITERT_HAS_AHWB_SUPPORT
  if (ahwb_ != nullptr) {
    LITERT_ASSIGN_OR_RETURN(void* data,
                            litert::internal::AhwbBuffer::Lock(ahwb_));
    return static_cast<T*>(data);
  }
#endif  // LITERT_HAS_AHWB_SUPPORT
  if (data_ == nullptr) {
    // Ensure the data is aligned.
    if (auto rc = posix_memalign(&data_, LITERT_HOST_MEMORY_BUFFER_ALIGNMENT,
                                 size_bytes_);
        rc) {
      return Unexpected(kLiteRtStatusErrorRuntimeFailure,
                        "Failed to allocate aligned memory");
    }
    if (auto status = tflite_gl_buffer_.Read(
            absl::MakeSpan(static_cast<T*>(data_), size_bytes_ / sizeof(T)));
        !status.ok()) {
      return Unexpected(
          kLiteRtStatusErrorRuntimeFailure,
          absl::StrCat("Failed to read GL buffer: ", status.message()));
    }
  }
  return Expected<T*>(static_cast<T*>(data_));
#else
  return Unexpected(kLiteRtStatusErrorRuntimeFailure,
                    "GlBuffer::Lock() is not supported");
#endif  // LITERT_HAS_OPENGL_SUPPORT
}

template <typename T>
Expected<void> GlBuffer::Unlock() {
#if LITERT_HAS_OPENGL_SUPPORT
  absl::MutexLock lock(&mutex_);
#if LITERT_HAS_AHWB_SUPPORT
  if (ahwb_ != nullptr) {
    return litert::internal::AhwbBuffer::Unlock(ahwb_);
  }
#endif  // LITERT_HAS_AHWB_SUPPORT
  if (data_ == nullptr) {
    return Error(
        kLiteRtStatusErrorRuntimeFailure,
        "Cannot unlock a buffer that wasn't locked in the first place");
  }
  if (auto status = tflite_gl_buffer_.Write(absl::MakeSpan(
          static_cast<const T*>(data_), size_bytes_ / sizeof(T)));
      !status.ok()) {
    return Unexpected(
        kLiteRtStatusErrorRuntimeFailure,
        absl::StrCat("Failed to write GL buffer: ", status.message()));
  }
  return Expected<void>();
#else
  return Unexpected(kLiteRtStatusErrorRuntimeFailure,
                    "GlBuffer::Unlock() is not supported");
#endif  // LITERT_HAS_OPENGL_SUPPORT
}

Expected<int> GlBuffer::CreateEglSyncAndFence() {
#if LITERT_HAS_OPENGL_SUPPORT && LITERT_HAS_AHWB_SUPPORT
  LITERT_RETURN_IF_ERROR(
      IsAhwbToGlInteropSupported(),
      Unexpected(kLiteRtStatusErrorRuntimeFailure,
                 "AHardwareBuffer to GL interop is not supported"));

  auto egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  LITERT_RETURN_IF_ERROR(egl_display != EGL_NO_DISPLAY,
                         Unexpected(kLiteRtStatusErrorRuntimeFailure,
                                    "Failed to get EGL display"));

  EGLSyncKHR egl_sync =
      eglCreateSyncKHR(egl_display, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
  LITERT_RETURN_IF_ERROR(
      egl_sync != EGL_NO_SYNC_KHR,
      Unexpected(kLiteRtStatusErrorRuntimeFailure,
                 "Failed to create EGL sync from AHardwareBuffer"));

  int native_fence = eglDupNativeFenceFDANDROID(egl_display, egl_sync);
  LITERT_RETURN_IF_ERROR(
      native_fence != -1,
      Unexpected(kLiteRtStatusErrorRuntimeFailure,
                 "Failed to dup native fence from AHardwareBuffer"));

  return native_fence;
#else
  return Unexpected(kLiteRtStatusErrorRuntimeFailure,
                    "AHardwareBuffer to GL interop is not supported");
#endif  // LITERT_HAS_OPENGL_SUPPORT && LITERT_HAS_AHWB_SUPPORT
}

}  // namespace internal
}  // namespace litert
