#
# Copyright 2018-2019, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import memoryoperations
from reorderexceptions import NotSupportedOperationException


class OperationFactory:
    """
    An abstract memory operation factory.

    This object factory puts special constraints on names of classes.
    It creates objects based on log in string format, as such the
    classes have to start with a capital letter and the rest of the
    name has to be in lowercase. For example::

        STORE -> Store
        FULL_REORDER -> Full_reorder

    The object to be created has to have and internal **Factory** class
    with a :func:`create` method taking a string parameter. For example see
    :class:`memoryoperations.Store`.

    :cvar __factories: The registered object factories.
    :type __factories: dict
    """
    __factories = {}
    __suffix = ['.BEGIN', '.END']
    memoryoperations.BaseOperation()

    @staticmethod
    def add_factory(id_, operation_factory):
        """
        Explicitly register an object factory.

        This method should be used when the factory cannot be inferred
        from the name of the object to be created.

        :param id_: The id under which this factory is to be registered
            in the dictionary.
        :type id_: str
        :param operation_factory: The operation factory to be registered.
        :return: None
        """
        OperationFactory.__factories[id_] = operation_factory

    @staticmethod
    def create_operation(string_operation, markers, stack):

        def check_marker_format(marker):
            """
            Checks if marker has proper suffix.
            """
            for s in OperationFactory.__suffix:
                if marker.endswith(s):
                    return

            raise NotSupportedOperationException(
                        "Incorrect marker format {}, suffix is missing."
                        .format(marker))

        def check_pair_consistency(stack, marker):
            """
            Checks if markers do not cross.
            You can pop from stack only if end
            marker match previous one.

            Example OK:
                MACRO1.BEGIN
                    MACRO2.BEGIN
                    MACRO2.END
                MACRO1.END

            Example NOT OK:
                MACRO1.BEGIN
                    MACRO2.BEGIN
                MACRO1.END
                    MACRO2.END
            """
            top = stack[-1][0]
            if top.endswith(OperationFactory.__suffix[0]):
                top = top[:-len(OperationFactory.__suffix[0])]
            if marker.endswith(OperationFactory.__suffix[-1]):
                marker = marker[:-len(OperationFactory.__suffix[-1])]

            if top != marker:
                raise NotSupportedOperationException(
                        "Cannot cross markers: {0}, {1}"
                        .format(top, marker))

        """
        Creates the object based on the pre-formatted string.

        The string needs to be in the specific format. Each specific value
        in the string has to be separated with a `;`. The first field
        has to be the name of the operation, the rest are operation
        specific values.

        :param string_operation: The string describing the operation.
        :param markers: The dict describing the pair marker-engine.
        :param stack: The stack describing the order of engine changes.
        :return: The specific object instantiated based on the string.
        """
        id_ = string_operation.split(";")[0]
        id_case_sensitive = id_.lower().capitalize()

        # checks if id_ is one of memoryoperation classes
        mem_ops = getattr(memoryoperations, id_case_sensitive, None)

        # if class is not one of memoryoperations
        # it means it can be user defined marker
        if mem_ops is None:
            check_marker_format(id_)
            # if id_ is section BEGIN
            if id_.endswith(OperationFactory.__suffix[0]):
                # BEGIN defined by user
                marker_name = id_.partition('.')[0]
                if markers is not None and marker_name in markers:
                    engine = markers[marker_name]
                    try:
                        mem_ops = getattr(memoryoperations, engine)
                    except AttributeError:
                        raise NotSupportedOperationException(
                                "Not supported reorder engine: {}"
                                .format(engine))
                # BEGIN but not defined by user
                else:
                    mem_ops = stack[-1][1]

                if issubclass(mem_ops, memoryoperations.ReorderBase):
                    stack.append((id_, mem_ops))

            # END section
            elif id_.endswith(OperationFactory.__suffix[-1]):
                check_pair_consistency(stack, id_)
                stack.pop()
                mem_ops = stack[-1][1]

        # here we have proper memory operation to perform,
        # it can be Store, Fence, ReorderDefault etc.
        id_ = mem_ops.__name__
        if id_ not in OperationFactory.__factories:
            OperationFactory.__factories[id_] = mem_ops.Factory()

        return OperationFactory.__factories[id_].create(string_operation)
