#ifndef PROTON_RAW_CONNECTION_H
#define PROTON_RAW_CONNECTION_H 1

/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <proton/condition.h>
#include <proton/event.h>
#include <proton/import_export.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @addtogroup proactor
 * @{
 */

/**
 * pn_raw_buffer_t is used to represent a single raw buffer in memory. It
 * holds the start of the  underlying memory in bytes and the length of the
 * full buffer in capacity. offset is the first byte in the buffer
 * that is to be written, size is used to represent the actual number of
 * bytes that are read/written in the buffer. If bytes is zero then no buffer is represented.
 *
 * @note The intent of the offset is to allow the actual bytes being read/written to be at a variable
 * location relative to the head of the buffer because of other data or structures that are important to the application
 * associated with the data to be written but not themselves read/written to the connection.
 */
typedef struct pn_raw_buffer_t {
    char *bytes;
    uint32_t capacity;
    uint32_t size;
    uint32_t offset;
} pn_raw_buffer_t;

/**
 * Create a new raw connection for use with the @ref proactor.
 * See @ref pn_proactor_raw_connect and @ref pn_listener_raw_accept.
 *
 * @note I expect this will be rarely used as those APIs can create a raw connection
 * themselves and the proactor will own the raw connection once they have been called.
 *
 * @note a reason this might be used is to create a raw connection and attach application
 * specific context to it before giving it to the proactor.
 */
PNP_EXTERN pn_raw_connection_t *pn_raw_connection(void);

/**
 * Close a raw connection.
 * This will close the underlying socket and release all buffers held by the raw connection.
 * It will cause @ref PN_RAW_BUFFERS_READ and @ref PN_RAW_BUFFERS_WRITTEN to be emitted so
 * the application can clean up buffers given to the raw connection. After that a
 * @ref PN_RAW_CONNECTION_DISCONNECTED event will be emitted to allow the application to clean up
 * any other state held by the raw connection.
 *
 */
PNP_EXTERN void pn_raw_connection_close(pn_raw_connection_t *connection);

/**
 * Free a raw connection.
 *
 * @note I expect this to be rarely used as the proactor API will usually free automatically
 * after the @ref PN_RAW_CONNECTION_DISCONNECTED event.
 *
 * @note Doing this when the raw connection still owns buffers will cause a memory leak. If this
 * is needed for whatever reason then @ref pn_raw_connection_close() should be used before
 * @ref pn_raw_connection_release_proactor() so that events are emitted to release the buffers.
 */
PNP_EXTERN void pn_raw_connection_free(pn_raw_connection_t *connection);

/**
 * Query the raw connection for how many more read buffers it can be given
 */
PNP_EXTERN size_t pn_raw_connection_read_buffers_capacity(pn_raw_connection_t *connection);

/**
 * Query the raw connection for how many more write buffers it can be given
 */
PNP_EXTERN size_t pn_raw_connection_write_buffers_capacity(pn_raw_connection_t *connection);

/**
 * Give the raw connection buffers to use for reading from the underlying socket.
 * If the raw socket has no read buffers then the application will never receive
 * a @ref PN_RAW_BUFFERS_READ event.
 *
 * A @ref PN_RAW_CONNECTION_NEED_READ_BUFFERS event will be generated immediately after
 * the @ref PN_RAW_CONNECTION_CONNECTED event if there are no read buffers. It will also be
 * generated whenever the raw connection runs out of read buffers. In both these cases the
 * event will not be generated again until @ref pn_raw_connection_give_read_buffers is called.
 *
 * @return the number of buffers actually given to the raw connection. This will only be different
 * from the number supplied if the connection has no more space to record more buffers. In this case
 * the buffers taken will be the earlier buffers in the array supplied, the elements 0 to the
 * returned value-1.
 *
 * @note The buffers given to the raw connection are owned by it until the application
 * receives a @ref PN_RAW_BUFFERS_READ event giving them back to the application. They must
 * not be accessed at all (written or even read) from calling @ref pn_raw_connection_give_read_buffers
 * until receiving this event.
 *
 * @note The application should not assume that the @ref PN_RAW_CONNECTION_NEED_READ_BUFFERS
 * event signifies that the connection is readable.
 */
PNP_EXTERN size_t pn_raw_connection_give_read_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t const *buffers, size_t num);

/**
 * Fetch buffers with bytes read from the raw socket
 *
 * @param[out] buffers pointer to an array of @ref pn_read_buffer_t structures which will be filled in with the read buffer information
 * @param[in] num the number of buffers allocated in the passed in array of buffers
 * @return the number of buffers being returned, if there is are no read bytes then this will be 0. As many buffers will be returned
 * as can be given the number that are passed in. So if the number returned is less than the number passed in there are no more buffers
 * read. But if the number is the same there may be more read buffers to take.
 *
 * @note After the application receives @ref PN_RAW_BUFFERS_READ there should be bytes read from the socket and
 * hence this call should return buffers. It is safe to carry on calling @ref pn_raw_connection_take_read_buffers
 * until it returns 0.
 */
PNP_EXTERN size_t pn_raw_connection_take_read_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t *buffers, size_t num);

/**
 * Give the raw connection buffers to write to the underlying socket.
 *
 * A @ref PN_RAW_BUFFERS_WRITTEN event will be generated once the buffers have been written to the socket
 * until this point the buffers must not be accessed at all (written or even read).
 *
 * A @ref PN_RAW_CONNECTION_NEED_WRITE_BUFFERS event will be generated immediately after
 * the @ref PN_RAW_CONNECTION_CONNECTED event if there are no write buffers. It will also be
 * generated whenever the raw connection finishes writing all the write buffers. In both these cases the
 * event will not be generated again until @ref pn_raw_connection_write is called.
 *
 * @return the number of buffers actually recorded by the raw connection to write. This will only be different
 * from the number supplied if the connection has no more space to record more buffers. In this case
 * the buffers recorded will be the earlier buffers in the array supplied, the elements 0 to the
 * returned value-1.
 *
 */
PNP_EXTERN size_t pn_raw_connection_write_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t const *buffers, size_t num);

/**
 * Return a buffer chain with buffers that have all been written to the raw socket
 *
 * @param[out] buffers pointer to an array of @ref pn_write_buffer_t structures which will be filled in with the written buffer information
 * @param[in] num the number of buffers allocated in the passed in array of buffers
 * @return the number of buffers being returned, if there is are no written buffers to return then this will be 0. As many buffers will be returned
 * as can be given the number that are passed in. So if the number returned is less than the number passed in there are no more buffers
 * written. But if the number is the same there may be more written buffers to take.
 *
 * @note After the application receives @ref PN_RAW_BUFFERS_WRITTEN there should be bytes written to the socket and
 * hence this call should return buffers. It is safe to carry on calling @ref pn_raw_connection_take_written_buffers
 * until it returns 0.
 */
PNP_EXTERN size_t pn_raw_connection_take_written_buffers(pn_raw_connection_t *connection, pn_raw_buffer_t *buffers, size_t num);

/*
 * Is the connection closed for read?
 */
PNP_EXTERN bool pn_raw_connection_is_read_closed(pn_raw_connection_t *connection);

/*
 * Is the connection closed for write?
 */
PNP_EXTERN bool pn_raw_connection_is_write_closed(pn_raw_connection_t *connection);

/**
 * Return a @ref PN_RAW_CONNECTION_WAKE event for @p connection as soon as possible.
 *
 * At least one wake event will be returned, serialized with other @ref proactor_events
 * for the same connection.  Wakes can be "coalesced" - if several
 * @ref pn_raw_connection_wake() calls happen close together, there may be only one
 * @ref PN_RAW_CONNECTION_WAKE event that occurs after all of them.
 *
 * @note If @p connection does not belong to a proactor, this call does nothing.
 *
 * @note Thread-safe
 */
PNP_EXTERN void pn_raw_connection_wake(pn_raw_connection_t *connection);

/**
 * Get additional information about a raw connection error.
 * There is a raw connection error if the @ref PN_RAW_CONNECTION_DISCONNECTED event
 * is received and the pn_condition_t associated is non null (@see pn_condition_is_set).
 *
 * The value returned is only valid until the end of handler for the
 * @ref PN_RAW_CONNECTION_DISCONNECTED event.
 */
PNP_EXTERN pn_condition_t *pn_raw_connection_condition(pn_raw_connection_t *connection);

/**
 * Get the application context associated with this raw connection.
 *
 * The application context for a raw connection may be set using
 * ::pn_raw_connection_set_context.
 *
 * @param[in] connection the raw connection whose context is to be returned.
 * @return the application context for the raw connection
 */
PNP_EXTERN void *pn_raw_connection_get_context(pn_raw_connection_t *connection);

/**
 * Set a new application context for a raw connection.
 *
 * The application context for a raw connection may be retrieved
 * using ::pn_raw_connection_get_context.
 *
 * @param[in] connection the raw connection object
 * @param[in] context the application context
 */
PNP_EXTERN void pn_raw_connection_set_context(pn_raw_connection_t *connection, void *context);

/**
 * Get the attachments that are associated with a raw connection.
 */
PNP_EXTERN pn_record_t *pn_raw_connection_attachments(pn_raw_connection_t *connection);

/**
 * Return the raw connection associated with an event.
 *
 * @return NULL if the event is not associated with a raw connection.
 */
PNP_EXTERN pn_raw_connection_t *pn_event_raw_connection(pn_event_t *event);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* raw_connection.h */