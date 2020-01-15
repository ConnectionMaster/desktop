# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .code_generator_info import CodeGeneratorInfo
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .function_like import FunctionLike
from .ir_map import IRMap
from .make_copy import make_copy
from .user_defined_type import UserDefinedType


class CallbackFunction(UserDefinedType, FunctionLike, WithExtendedAttributes,
                       WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-callback-functions"""

    class IR(IRMap.IR, FunctionLike.IR, WithExtendedAttributes,
             WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     arguments,
                     return_type,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            IRMap.IR.__init__(
                self,
                identifier=identifier,
                kind=IRMap.IR.Kind.CALLBACK_FUNCTION)
            FunctionLike.IR.__init__(
                self,
                identifier=identifier,
                arguments=arguments,
                return_type=return_type)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

    def __init__(self, ir):
        assert isinstance(ir, CallbackFunction.IR)

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        FunctionLike.__init__(self, ir)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

    # UserDefinedType overrides
    @property
    def is_callback_function(self):
        return True
