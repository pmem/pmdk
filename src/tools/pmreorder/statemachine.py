# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

import memoryoperations as memops
import reorderengines
from reorderexceptions import InconsistentFileException
from reorderexceptions import NotSupportedOperationException


class State:
    """
    The base class of all states.

    :ivar _context: The reordering context.
    :type _context: opscontext.OpsContext
    :ivar trans_stores: The list of unflushed stores.
    :type trans_stores: list of :class:`memoryoperations.Store`
    """
    trans_stores = []

    def __init__(self, context):
        """
        Default state constructor.

        :param context: The context of the reordering.
        :type context: opscontext.OpsContext
        """
        self._context = context

    def next(self, in_op):
        """
        Go to the next state based on the input.

        :Note:
            The next state might in fact be the same state.

        :param in_op: The state switch trigger operation.
        :type in_op: subclass of :class:`memoryoperations.BaseOperation`
        :return: The next state.
        :rtype: subclass of :class:`State`
        """
        raise NotImplementedError

    def run(self, in_op):
        """
        Perform the required operation in this state.

        :param in_op: The operation to be performed in this state.
        :type in_op: subclass of :class:`memoryoperations.BaseOperation`
        :return: None
        """
        raise NotImplementedError


class InitState(State):
    """
    The initial no-op state.
    """
    def __init__(self, context):
        """
        Saves the reordering context.

        :param context: The reordering context.
        :type context: opscontext.OpsContext
        """
        super(InitState, self).__init__(context)

    def next(self, in_op):
        """
        Switch to the next valid state.

        :param in_op: Ignored.
        :return: The next valid state.
        :rtype: CollectingState
        """
        return CollectingState(self._context)

    def run(self, in_op):
        """
        Does nothing.

        :param in_op: Ignored.
        :return: always True
        """
        return True


class CollectingState(State):
    """
    Collects appropriate operations.

    This state mostly aggregates stores and flushes. It also
    validates which stores will be made persistent and passes
    them on to the next state.

    :ivar _ops_list: The list of collected stores.
    :type _ops_list: list of :class:`memoryoperations.Store`
    :ivar _inner_state: The internal state of operations.
    :type _inner_state: str
    """
    def __init__(self, context):
        """
        Saves the reordering context.

        :param context: The reordering context.
        :type context: opscontext.OpsContext
        """
        super(CollectingState, self).__init__(context)
        self._ops_list = []
        self._ops_list += State.trans_stores
        self._inner_state = "init"

    def next(self, in_op):
        """
        Switch to the next valid state.

        :param in_op: The state switch trigger operation.
        :type in_op: subclass of :class:`memoryoperations.BaseOperation`
        :return: The next state.
        :rtype: subclass of :class:`State`
        """
        if isinstance(in_op, memops.Fence) and \
                self._inner_state == "flush":
            return ReplayingState(self._ops_list, self._context)
        else:
            return self

    def run(self, in_op):
        """
        Perform operations in this state.

        Based on the type of operation, different handling is employed.
        The recognized and handled types of operations are:

            * :class:`memoryoperations.ReorderBase`
            * :class:`memoryoperations.FlushBase`
            * :class:`memoryoperations.Store`
            * :class:`memoryoperations.Register_file`

        :param in_op: The operation to be performed in this state.
        :type in_op: subclass of :class:`memoryoperations.BaseOperation`
        :return: always True
        """
        self.move_inner_state(in_op)
        if isinstance(in_op, memops.ReorderBase):
            self.substitute_reorder(in_op)
        elif isinstance(in_op, memops.FlushBase):
            self.flush_stores(in_op)
        elif isinstance(in_op, memops.Store):
            self._ops_list.append(in_op)
        elif isinstance(in_op, memops.Register_file):
            self.reg_file(in_op)

        return True

    def substitute_reorder(self, order_ops):
        """
        Changes the reordering engine based on the log marker class.

        :param order_ops: The reordering marker class.
        :type order_ops: subclass of :class:`memoryoperations.ReorderBase`
        :return: None
        """
        if isinstance(order_ops, memops.ReorderFull):
            self._context.reorder_engine = \
                reorderengines.FullReorderEngine()
            self._context.test_on_barrier = \
                self._context.reorder_engine.test_on_barrier
        elif isinstance(order_ops, memops.ReorderPartial):
            # TODO add macro in valgrind or
            # parameter inside the tool to support parameters?
            self._context.reorder_engine = \
                 reorderengines.RandomPartialReorderEngine(3)
            self._context.test_on_barrier = \
                self._context.reorder_engine.test_on_barrier
        elif isinstance(order_ops, memops.ReorderAccumulative):
            self._context.reorder_engine = \
                reorderengines.AccumulativeReorderEngine()
            self._context.test_on_barrier = \
                self._context.reorder_engine.test_on_barrier
        elif isinstance(order_ops, memops.ReorderReverseAccumulative):
            self._context.reorder_engine = \
                reorderengines.AccumulativeReverseReorderEngine()
            self._context.test_on_barrier = \
                self._context.reorder_engine.test_on_barrier
        elif isinstance(order_ops, memops.NoReorderDoCheck):
            self._context.reorder_engine = reorderengines.NoReorderEngine()
            self._context.test_on_barrier = \
                self._context.reorder_engine.test_on_barrier
        elif isinstance(order_ops, memops.NoReorderNoCheck):
            self._context.reorder_engine = reorderengines.NoCheckerEngine()
            self._context.test_on_barrier = \
                self._context.reorder_engine.test_on_barrier
        elif isinstance(order_ops, memops.ReorderDefault):
            self._context.reorder_engine = self._context.default_engine
            self._context.test_on_barrier = self._context.default_barrier
        else:
            raise NotSupportedOperationException(
                                   "Not supported reorder engine: {}"
                                   .format(order_ops))

    def flush_stores(self, flush_op):
        """
        Marks appropriate stores as flushed.

        Does not align the flush, the log is expected to have the
        flushes properly aligned.

        :param flush_op: The flush operation marker.
        :type flush_op: subclass of :class:`memoryoperations.FlushBase`
        :return: None
        """
        for st in self._ops_list:
            if flush_op.is_in_flush(st):
                st.flushed = True

    def reg_file(self, file_op):
        """
        Register a new file mapped into virtual memory.

        :param file_op: File registration operation marker.
        :type file_op: memoryoperations.Register_file
        :return: None
        """
        self._context.file_handler.add_file(file_op.name,
                                            file_op.address,
                                            file_op.size)

    def move_inner_state(self, in_op):
        """
        Tracks the internal state of the collection.

        The collected stores need to be processed only at specific moments -
        after full persistent memory barriers (flush-fence).

        :param in_op: The performed operation.
        :type in_op: subclass of :class:`memoryoperations.BaseOperation`
        :return: None
        """
        if isinstance(in_op, memops.Store) and \
                self._inner_state == "init":
            self._inner_state = "dirty"
        elif isinstance(in_op, memops.FlushBase) and \
                self._inner_state == "dirty":
            self._inner_state = "flush"
        elif isinstance(in_op, memops.Fence) and \
                self._inner_state == "flush":
            self._inner_state = "fence"
        elif isinstance(in_op, memops.Flush) and \
                self._inner_state == "init":
            self._inner_state = "flush"


class ReplayingState(State):
    """
    Replays all collected stores according to the reordering context.

    :ivar _ops_list: The list of stores to be reordered and replayed.
    :type _ops_list: list of :class:`memoryoperations.Store`
    """
    def __init__(self, in_ops_list, context):
        """

        :param in_ops_list:
        :param context:
        :return:
        """
        super(ReplayingState, self).__init__(context)
        self._ops_list = in_ops_list

    def next(self, in_op):
        """
        Switches to the collecting state regardless of the input.

        :param in_op: Ignored.
        :type in_op: subclass of :class:`memoryoperations.BaseOperation`
        :return: The next state.
        :rtype: CollectingState
        """
        return CollectingState(self._context)

    def run(self, in_op):
        """
        Perform operations in this state.

        The replaying state performs reordering and if necessary checks
        the consistency of the registered files. The decisions and
        type of reordering to be used is defined by the context.

        :param in_op: The operation to be performed in this state.
        :type in_op: subclass of :class:`memoryoperations.BaseOperation`
        :return: State of consistency check.
        """
        # specifies consistency state of sequence
        consistency = True

        # consider only flushed stores
        flushed_stores = list(filter(lambda x: x.flushed, self._ops_list))

        # not-flushed stores should be passed to next state
        State.trans_stores = list(filter(lambda x: x.flushed is False,
                                         self._ops_list))

        if self._context.test_on_barrier:
            for seq in self._context.reorder_engine.generate_sequence(
                                                              flushed_stores):
                for op in seq:
                    # do stores
                    self._context.file_handler.do_store(op)
                # check consistency of all files
                try:
                    self._context.file_handler.check_consistency()
                except InconsistentFileException as e:
                    consistency = False
                    self._context.logger.warning(e)
                    stacktrace = "Call trace:\n"
                    for num, op in enumerate(seq):
                        stacktrace += "Store [{}]:\n".format(num)
                        stacktrace += str(op.trace)
                    self._context.logger.warning(stacktrace)

                for op in reversed(seq):
                    # revert the changes
                    self._context.file_handler.do_revert(op)
        # write all flushed stores
        for op in flushed_stores:
            self._context.file_handler.do_store(op)

        return consistency


class StateMachine:
    """
    The state machine driver.

    :ivar _curr_state: The current state.
    :type _curr_state: subclass of :class:`State`
    """
    def __init__(self, init_state):
        """
        Initialize the state machine with a specified state.

        :param init_state: The initial state to be used.
        :type init_state: subclass of :class:`State`
        """
        self._curr_state = init_state

    def run_all(self, operations):
        """
        Starts the state machine.

        :param operations: The operations to be performed by the state
            machine.
        :type operations: list of :class:`memoryoperations.BaseOperation`
        :return: None
        """
        all_consistent = True
        for ops in operations:
            self._curr_state = self._curr_state.next(ops)
            check = self._curr_state.run(ops)
            if check is False:
                all_consistent = check

        return all_consistent
