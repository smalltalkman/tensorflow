# Copyright 2023 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================

from typing import ClassVar, overload

class Layout:
    @overload
    def __init__(self, layout: Layout) -> None: ...
    @overload
    def __init__(self, type: LayoutType, sharding_specs: list[str], mesh: Mesh) -> None: ...
    @overload
    def __init__(self, layout_proto) -> None: ...
    @overload
    def __init__(self, layout_str: str) -> None: ...
    @overload
    def __init__(self, mesh: Mesh, rank: int) -> None: ...
    @overload
    def __init__(self, mesh: Mesh, rank: int, batch_dim: str, axis: int) -> None: ...
    @overload
    def __init__(self, mesh: Mesh) -> None: ...
    def as_proto(self, *args, **kwargs): ...
    def global_shape_from_local_shape(self, local_shape: list[int]) -> tuple: ...
    def is_batch_parallel(self) -> bool: ...
    def is_fully_replicated(self) -> bool: ...
    def is_single_device(self) -> bool: ...
    def local_shape_from_global_shape(self, global_shape: list[int]) -> tuple: ...
    def num_shards(self, idx: int) -> int: ...
    def to_parted(self) -> Layout: ...
    def to_string(self) -> str: ...
    def __eq__(self, arg0: Layout) -> bool: ...
    @property
    def mesh(self) -> Mesh: ...
    @property
    def rank(self) -> int: ...
    @property
    def sharding_specs(self) -> list[str]: ...
    @property
    def type(self) -> LayoutType: ...

class LayoutType:
    __members__: ClassVar[dict] = ...  # read-only
    PARTED: ClassVar[LayoutType] = ...
    SINGLE_DEVICE: ClassVar[LayoutType] = ...
    STATIC: ClassVar[LayoutType] = ...
    __entries: ClassVar[dict] = ...
    def __init__(self, value: int) -> None: ...
    def __eq__(self, other: object) -> bool: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: object) -> bool: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class Mesh:
    @overload
    def __init__(self, mesh: Mesh) -> None: ...
    @overload
    def __init__(self, arg0: str, arg1: list[str], arg2: list[int], arg3: list[int], arg4: list[str], arg5: list[int], arg6: list[str], arg7: bool) -> None: ...
    @overload
    def __init__(self, single_device: str) -> None: ...
    @overload
    def __init__(self, mesh_proto) -> None: ...
    @overload
    def __init__(self, mesh_str: str) -> None: ...
    def as_proto(self, *args, **kwargs): ...
    def contains_dim(self, dim_name: str) -> bool: ...
    def device_location(self, arg0: int) -> list[int]: ...
    def device_type(self) -> str: ...
    def dim_size(self, dim_name: str) -> int: ...
    def global_device_ids(self) -> Sequence[int]: ...
    def global_devices(self) -> list[str]: ...
    def host_mesh(self) -> Mesh: ...
    def is_remote(self) -> bool: ...
    def is_single_device(self) -> bool: ...
    def local_device_ids(self) -> Sequence[int]: ...
    def local_devices(self) -> Sequence[str]: ...
    def min_global_device_id(self) -> int: ...
    def num_local_devices(self) -> int: ...
    def shape(self) -> list[int]: ...
    def to_string(self) -> str: ...
    def use_xla_spmd(self) -> bool: ...
    def __contains__(self, dim_name: str) -> bool: ...
    def __eq__(self, arg0: Mesh) -> bool: ...
    @property
    def dim_names(self) -> list[str]: ...
    @property
    def name(self) -> str: ...
    @property
    def single_device(self) -> str: ...
    @property
    def size(self) -> int: ...

def AddMesh(arg0, arg1: str, arg2: bool) -> None: ...
def Allocate(arg0: str, arg1: bool, arg2: int) -> object: ...
def ClearTPUCoreIDs(arg0) -> None: ...
def ExperimentalClearDefaultLayout(arg0) -> None: ...
def ExperimentalClearDefaultMesh(arg0) -> None: ...
def ExperimentalSetDefaultLayout(arg0, arg1: str) -> None: ...
def ExperimentalSetDefaultMesh(arg0, arg1: str) -> None: ...
def FetchLayout(arg0: object, arg1: object, arg2) -> object: ...
def GetStats(arg0: object, arg1) -> dict[str, int]: ...
def IsDTensor(arg0: object, arg1: object, arg2) -> bool: ...
def IsSparseDTensor(arg0: object, arg1: object, arg2) -> bool: ...
def Pack(arg0: object, arg1: object, arg2: str, arg3, arg4: bool) -> object: ...
def SetIteratorElementLayouts(arg0: object, arg1: object, arg2: list[str], arg3) -> None: ...
def SetTPUCoreIDs(arg0, arg1: str, arg2: list[int]) -> None: ...
def TPUCoreIDsToLocations(arg0: object, arg1, arg2: list[int]) -> list[list[int]]: ...
def TPUCoreLocationsToIDs(arg0: object, arg1, arg2: list[list[int]]) -> list[int]: ...
def Unpack(arg0: object, arg1: object, arg2) -> object: ...
