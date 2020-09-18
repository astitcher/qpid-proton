#ifndef PROTON_RAW_DATAGRAM_H
#define PROTON_RAW_DATAGRAM_H 1

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
#include <proton/netaddr.h>
#include <proton/types.h>
#include <proton/raw_connection.h>

#include <stdint.h>

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
  #endif

  /**
   * @file
   *
   * @addtogroup raw_connection
   * @{
   */

  /**
   * Used to represent datagram (send/recv) addresses
   */
  typedef union pn_raw_addr_t {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } pn_raw_addr_t;

  /**
   * A descriptor used to represent a single raw buffer in memory.
   *
   * @note The intent of the offset is to allow the actual bytes being read/written to be at a variable
   * location relative to the head of the buffer because of other data or structures that are important to the application
   * associated with the data to be written but not themselves read/written to the connection.
   *
   * @note For read buffers: When read buffers are returned to the application size will be the number of bytes read.
   * Read operations will not change the context, bytes or capacity members of the structure.
   *
   * @note For write buffers: When write buffers are returned to the application all of the struct members will be returned
   * unaltered. Also write operations will not modify the bytes of the buffer passed in at all. In principle this means that
   * the write buffer can be used for multiple writes at the same time as long as the actual buffer is unmodified by the
   * application at any time the buffer is being used by any raw datagram.
   */
  typedef struct pn_raw_datagram_buffer_t {
    uintptr_t context; /**< Used to associate arbitrary application data with this raw buffer */
    char *bytes; /**< Pointer to the start of the raw buffer, if this is null then no buffer is represented */
    uint32_t capacity; /**< Count of available bytes starting at @ref bytes */
    uint32_t size; /**< Number of bytes read or to be written starting at @ref offset */
    uint32_t offset; /**< First byte in the buffer to be read or written */
    pn_raw_addr_t address; /**< Either the from address for a receive or the to address for a send */
  } pn_raw_datagram_buffer_t;


  /**
   * Create a new raw datagram for use with the @ref proactor.
   * See @ref pn_proactor_raw_datagram.
   *
   * @return A newly allocated pn_raw_datagram_t or NULL if there wasn't sufficient memory.
   *
   * @note This is the only pn_raw_datagram_t function that allocates memory. So an application that
   * wants good control of out of memory conditions should check the return value for NULL.
   *
   * @note It would be a good practice to create a raw datagram and attach application
   * specific context to it before giving it to the proactor.
   *
   * @note There is no way to free a pn_raw_datagram_t as once passed to the proactor the proactor owns
   * it and controls its lifecycle.
   */
  PNP_EXTERN pn_raw_datagram_t *pn_raw_datagram(void);

  /**
   * Get the local address of a raw datagram. Return `NULL` if not available.
   * Pointer is invalid after the transport closes (@ref PN_RAW_DATAGRAM_CLOSED event is handled)
   */
  PNP_EXTERN const pn_raw_addr_t *pn_raw_datagram_local_addr(pn_raw_datagram_t *datagram);

  /**
   * Close a raw datagram.
   * This will close the underlying socket and release all buffers held by the raw datagram.
   * It will cause @ref PN_RAW_DATAGRAM_READ and @ref PN_RAW_DATAGRAM_WRITTEN to be emitted so
   * the application can clean up buffers given to the raw datagram. After that a
   * @ref PN_RAW_DATAGRAM_CLOSED event will be emitted to allow the application to clean up
   * any other state held by the raw datagram.
   *
   */
  PNP_EXTERN void pn_raw_datagram_close(pn_raw_datagram_t *datagram);

  /**
   * Query the raw datagram for how many more read buffers it can be given
   */
  PNP_EXTERN size_t pn_raw_datagram_recv_buffers_capacity(pn_raw_datagram_t *datagram);

  /**
   * Query the raw datagram for how many more write buffers it can be given
   */
  PNP_EXTERN size_t pn_raw_datagram_send_buffers_capacity(pn_raw_datagram_t *datagram);

  /**
   * Give the raw datagram buffers to use for reading from the underlying socket.
   * If the raw socket has no read buffers then the application will never receive
   * a @ref PN_RAW_DATAGRAM_READ event.
   *
   * A @ref PN_RAW_DATAGRAM_NEED_READ_BUFFERS event will be generated immediately after
   * the @ref PN_RAW_DATAGRAM_CONNECTED event if there are no read buffers. It will also be
   * generated whenever the raw datagram runs out of read buffers. In both these cases the
   * event will not be generated again until @ref pn_raw_datagram_give_read_buffers is called.
   *
   * @return the number of buffers actually given to the raw datagram. This will only be different
   * from the number supplied if the datagram has no more space to record more buffers. In this case
   * the buffers taken will be the earlier buffers in the array supplied, the elements 0 to the
   * returned value-1.
   *
   * @note The buffers given to the raw datagram are owned by it until the application
   * receives a @ref PN_RAW_DATAGRAM_READ event giving them back to the application. They must
   * not be accessed at all (written or even read) from calling @ref pn_raw_datagram_give_read_buffers
   * until receiving this event.
   *
   * @note The application should not assume that the @ref PN_RAW_DATAGRAM_NEED_READ_BUFFERS
   * event signifies that the datagram is readable.
   */
  PNP_EXTERN size_t pn_raw_datagram_give_recv_buffers(pn_raw_datagram_t *datagram, pn_raw_datagram_buffer_t const *buffers, size_t num);

  /**
   * Fetch buffers with bytes read from the raw socket
   *
   * @param[out] buffers pointer to an array of @ref pn_raw_datagram_buffer_t structures which will be filled in with the read buffer information
   * @param[in] num the number of buffers allocated in the passed in array of buffers
   * @return the number of buffers being returned, if there is are no read bytes then this will be 0. As many buffers will be returned
   * as can be given the number that are passed in. So if the number returned is less than the number passed in there are no more buffers
   * read. But if the number is the same there may be more read buffers to take.
   *
   * @note After the application receives @ref PN_RAW_DATAGRAM_READ there should be bytes read from the socket and
   * hence this call should return buffers. It is safe to carry on calling @ref pn_raw_datagram_take_read_buffers
   * until it returns 0.
   */
  PNP_EXTERN size_t pn_raw_datagram_take_recv_buffers(pn_raw_datagram_t *datagram, pn_raw_datagram_buffer_t *buffers, size_t num);

  /**
   * Give the raw datagram buffers to write to the underlying socket.
   *
   * A @ref PN_RAW_DATAGRAM_WRITTEN event will be generated once the buffers have been written to the socket
   * until this point the buffers must not be accessed at all (written or even read).
   *
   * A @ref PN_RAW_DATAGRAM_NEED_WRITE_BUFFERS event will be generated immediately after
   * the @ref PN_RAW_DATAGRAM_CONNECTED event if there are no write buffers. It will also be
   * generated whenever the raw datagram finishes writing all the write buffers. In both these cases the
   * event will not be generated again until @ref pn_raw_datagram_write_buffers is called.
   *
   * @return the number of buffers actually recorded by the raw datagram to write. This will only be different
   * from the number supplied if the datagram has no more space to record more buffers. In this case
   * the buffers recorded will be the earlier buffers in the array supplied, the elements 0 to the
   * returned value-1.
   *
   */
  PNP_EXTERN size_t pn_raw_datagram_send_buffers(pn_raw_datagram_t *datagram, pn_raw_datagram_buffer_t const *buffers, size_t num);

  /**
   * Return a buffer chain with buffers that have all been written to the raw socket
   *
   * @param[out] buffers pointer to an array of @ref pn_raw_datagram_buffer_t structures which will be filled in with the written buffer information
   * @param[in] num the number of buffers allocated in the passed in array of buffers
   * @return the number of buffers being returned, if there is are no written buffers to return then this will be 0. As many buffers will be returned
   * as can be given the number that are passed in. So if the number returned is less than the number passed in there are no more buffers
   * written. But if the number is the same there may be more written buffers to take.
   *
   * @note After the application receives @ref PN_RAW_DATAGRAM_WRITTEN there should be bytes written to the socket and
   * hence this call should return buffers. It is safe to carry on calling @ref pn_raw_datagram_take_written_buffers
   * until it returns 0.
   */
  PNP_EXTERN size_t pn_raw_datagram_take_sent_buffers(pn_raw_datagram_t *datagram, pn_raw_datagram_buffer_t *buffers, size_t num);

  /**
   * Is @p datagram closed?
   *
   * @return true if the raw datagram is closed for read.
   */
  PNP_EXTERN bool pn_raw_datagram_is_closed(pn_raw_datagram_t *datagram);

  /**
   * Return a @ref PN_RAW_DATAGRAM_WAKE event for @p datagram as soon as possible.
   *
   * At least one wake event will be returned, serialized with other @ref proactor_events
   * for the same raw datagram.  Wakes can be "coalesced" - if several
   * @ref pn_raw_datagram_wake() calls happen close together, there may be only one
   * @ref PN_RAW_DATAGRAM_WAKE event that occurs after all of them.
   *
   * @note Thread-safe
   */
  PNP_EXTERN void pn_raw_datagram_wake(pn_raw_datagram_t *datagram);

  /**
   * Get additional information about a raw datagram error.
   * There is a raw datagram error if the @ref PN_RAW_DATAGRAM_CLOSED event
   * is received and the pn_condition_t associated is non null (@see pn_condition_is_set).
   *
   * The value returned is only valid until the end of handler for the
   * @ref PN_RAW_DATAGRAM_CLOSED event.
   */
  PNP_EXTERN pn_condition_t *pn_raw_datagram_condition(pn_raw_datagram_t *datagram);

  /**
   * Get the application context associated with this raw datagram.
   *
   * The application context for a raw datagram may be set using
   * ::pn_raw_datagram_set_context.
   *
   * @param[in] datagram the raw datagram whose context is to be returned.
   * @return the application context for the raw datagram
   */
  PNP_EXTERN void *pn_raw_datagram_get_context(pn_raw_datagram_t *datagram);

  /**
   * Set a new application context for a raw datagram.
   *
   * The application context for a raw datagram may be retrieved
   * using ::pn_raw_datagram_get_context.
   *
   * @param[in] datagram the raw datagram object
   * @param[in] context the application context
   */
  PNP_EXTERN void pn_raw_datagram_set_context(pn_raw_datagram_t *datagram, void *context);

  /**
   * Get the attachments that are associated with a raw datagram.
   */
  PNP_EXTERN pn_record_t *pn_raw_datagram_attachments(pn_raw_datagram_t *datagram);

  /**
   * Return the raw datagram associated with an event.
   *
   * @return NULL if the event is not associated with a raw datagram.
   */
  PNP_EXTERN pn_raw_datagram_t *pn_event_raw_datagram(pn_event_t *event);


  /**
   * @}
   */

  #ifdef __cplusplus
}
#endif

#endif /* raw_datagram.h */

