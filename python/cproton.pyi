# /*
#  *
#  * Licensed to the Apache Software Foundation (ASF) under one
#  * or more contributor license agreements.  See the NOTICE file
#  * distributed with this work for additional information
#  * regarding copyright ownership.  The ASF licenses this file
#  * to you under the Apache License, Version 2.0 (the
#  * "License"); you may not use this file except in compliance
#  * with the License.  You may obtain a copy of the License at
#  *
#  *   http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing,
#  * software distributed under the License is distributed on an
#  * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  * KIND, either express or implied.  See the License for the
#  * specific language governing permissions and limitations
#  * under the License.
#  *
#  */

from typing import Any, NewType, TypeAlias

from cproton_ffi import ffi

# from cproton_ffi.lib import (
#     PN_ACCEPTED as PN_ACCEPTED,
#     PN_ARRAY as PN_ARRAY,
#     PN_BOOL as PN_BOOL,
#     PN_BYTE as PN_BYTE,
#     PN_CHAR as PN_CHAR,
#     PN_CONFIGURATION as PN_CONFIGURATION,
#     PN_CONNECTION_BOUND as PN_CONNECTION_BOUND,
#     PN_CONNECTION_FINAL as PN_CONNECTION_FINAL,
#     PN_CONNECTION_INIT as PN_CONNECTION_INIT,
#     PN_CONNECTION_LOCAL_CLOSE as PN_CONNECTION_LOCAL_CLOSE,
#     PN_CONNECTION_LOCAL_OPEN as PN_CONNECTION_LOCAL_OPEN,
#     PN_CONNECTION_REMOTE_CLOSE as PN_CONNECTION_REMOTE_CLOSE,
#     PN_CONNECTION_REMOTE_OPEN as PN_CONNECTION_REMOTE_OPEN,
#     PN_CONNECTION_UNBOUND as PN_CONNECTION_UNBOUND,
#     PN_COORDINATOR as PN_COORDINATOR,
#     PN_DECIMAL128 as PN_DECIMAL128,
#     PN_DECIMAL32 as PN_DECIMAL32,
#     PN_DECIMAL64 as PN_DECIMAL64,
#     PN_DEFAULT_PRIORITY as PN_DEFAULT_PRIORITY,
#     PN_DELIVERIES as PN_DELIVERIES,
#     PN_DELIVERY as PN_DELIVERY,
#     PN_DESCRIBED as PN_DESCRIBED,
#     PN_DIST_MODE_COPY as PN_DIST_MODE_COPY,
#     PN_DIST_MODE_MOVE as PN_DIST_MODE_MOVE,
#     PN_DIST_MODE_UNSPECIFIED as PN_DIST_MODE_UNSPECIFIED,
#     PN_DOUBLE as PN_DOUBLE,
#     PN_EOS as PN_EOS,
#     PN_EVENT_NONE as PN_EVENT_NONE,
#     PN_EXPIRE_NEVER as PN_EXPIRE_NEVER,
#     PN_EXPIRE_WITH_CONNECTION as PN_EXPIRE_WITH_CONNECTION,
#     PN_EXPIRE_WITH_LINK as PN_EXPIRE_WITH_LINK,
#     PN_EXPIRE_WITH_SESSION as PN_EXPIRE_WITH_SESSION,
#     PN_FLOAT as PN_FLOAT,
#     PN_INTR as PN_INTR,
#     PN_LINK_FINAL as PN_LINK_FINAL,
#     PN_LINK_FLOW as PN_LINK_FLOW,
#     PN_LINK_INIT as PN_LINK_INIT,
#     PN_LINK_LOCAL_CLOSE as PN_LINK_LOCAL_CLOSE,
#     PN_LINK_LOCAL_DETACH as PN_LINK_LOCAL_DETACH,
#     PN_LINK_LOCAL_OPEN as PN_LINK_LOCAL_OPEN,
#     PN_LINK_REMOTE_CLOSE as PN_LINK_REMOTE_CLOSE,
#     PN_LINK_REMOTE_DETACH as PN_LINK_REMOTE_DETACH,
#     PN_LINK_REMOTE_OPEN as PN_LINK_REMOTE_OPEN,
#     PN_LIST as PN_LIST,
#     PN_LOCAL_ACTIVE as PN_LOCAL_ACTIVE,
#     PN_LOCAL_CLOSED as PN_LOCAL_CLOSED,
#     PN_LOCAL_UNINIT as PN_LOCAL_UNINIT,
#     PN_MAP as PN_MAP,
#     PN_MODIFIED as PN_MODIFIED,
#     PN_NONDURABLE as PN_NONDURABLE,
#     PN_OVERFLOW as PN_OVERFLOW,
#     PN_RCV_FIRST as PN_RCV_FIRST,
#     PN_RCV_SECOND as PN_RCV_SECOND,
#     PN_RECEIVED as PN_RECEIVED,
#     PN_REJECTED as PN_REJECTED,
#     PN_RELEASED as PN_RELEASED,
#     PN_REMOTE_ACTIVE as PN_REMOTE_ACTIVE,
#     PN_REMOTE_CLOSED as PN_REMOTE_CLOSED,
#     PN_REMOTE_UNINIT as PN_REMOTE_UNINIT,
#     PN_SASL_AUTH as PN_SASL_AUTH,
#     PN_SASL_NONE as PN_SASL_NONE,
#     PN_SASL_OK as PN_SASL_OK,
#     PN_SASL_PERM as PN_SASL_PERM,
#     PN_SASL_SYS as PN_SASL_SYS,
#     PN_SASL_TEMP as PN_SASL_TEMP,
#     PN_SESSION_FINAL as PN_SESSION_FINAL,
#     PN_SESSION_INIT as PN_SESSION_INIT,
#     PN_SESSION_LOCAL_CLOSE as PN_SESSION_LOCAL_CLOSE,
#     PN_SESSION_LOCAL_OPEN as PN_SESSION_LOCAL_OPEN,
#     PN_SESSION_REMOTE_CLOSE as PN_SESSION_REMOTE_CLOSE,
#     PN_SESSION_REMOTE_OPEN as PN_SESSION_REMOTE_OPEN,
#     PN_SHORT as PN_SHORT,
#     PN_SND_MIXED as PN_SND_MIXED,
#     PN_SND_SETTLED as PN_SND_SETTLED,
#     PN_SND_UNSETTLED as PN_SND_UNSETTLED,
#     PN_SOURCE as PN_SOURCE,
#     PN_SSL_ANONYMOUS_PEER as PN_SSL_ANONYMOUS_PEER,
#     PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY as PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY,
#     PN_SSL_CERT_SUBJECT_COMMON_NAME as PN_SSL_CERT_SUBJECT_COMMON_NAME,
#     PN_SSL_CERT_SUBJECT_COUNTRY_NAME as PN_SSL_CERT_SUBJECT_COUNTRY_NAME,
#     PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME as PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME,
#     PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT as PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT,
#     PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE as PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE,
#     PN_SSL_MD5 as PN_SSL_MD5,
#     PN_SSL_MODE_CLIENT as PN_SSL_MODE_CLIENT,
#     PN_SSL_MODE_SERVER as PN_SSL_MODE_SERVER,
#     PN_SSL_RESUME_NEW as PN_SSL_RESUME_NEW,
#     PN_SSL_RESUME_REUSED as PN_SSL_RESUME_REUSED,
#     PN_SSL_RESUME_UNKNOWN as PN_SSL_RESUME_UNKNOWN,
#     PN_SSL_SHA1 as PN_SSL_SHA1,
#     PN_SSL_SHA256 as PN_SSL_SHA256,
#     PN_SSL_SHA512 as PN_SSL_SHA512,
#     PN_SSL_VERIFY_PEER as PN_SSL_VERIFY_PEER,
#     PN_SSL_VERIFY_PEER_NAME as PN_SSL_VERIFY_PEER_NAME,
#     PN_SYMBOL as PN_SYMBOL,
#     PN_TARGET as PN_TARGET,
#     PN_TIMEOUT as PN_TIMEOUT,
#     PN_TIMER_TASK as PN_TIMER_TASK,
#     PN_TIMESTAMP as PN_TIMESTAMP,
#     PN_TRACE_DRV as PN_TRACE_DRV,
#     PN_TRACE_FRM as PN_TRACE_FRM,
#     PN_TRACE_OFF as PN_TRACE_OFF,
#     PN_TRACE_RAW as PN_TRACE_RAW,
#     PN_TRANSPORT as PN_TRANSPORT,
#     PN_TRANSPORT_CLOSED as PN_TRANSPORT_CLOSED,
#     PN_TRANSPORT_ERROR as PN_TRANSPORT_ERROR,
#     PN_TRANSPORT_HEAD_CLOSED as PN_TRANSPORT_HEAD_CLOSED,
#     PN_TRANSPORT_TAIL_CLOSED as PN_TRANSPORT_TAIL_CLOSED,
#     PN_UBYTE as PN_UBYTE,
#     PN_UINT as PN_UINT,
#     PN_UNSPECIFIED as PN_UNSPECIFIED,
#     PN_USHORT as PN_USHORT,
#     PN_VERSION_MAJOR as PN_VERSION_MAJOR,
#     PN_VERSION_MINOR as PN_VERSION_MINOR,
#     PN_VERSION_POINT as PN_VERSION_POINT,
#     pn_cast_pn_connection as pn_cast_pn_connection,
#     pn_cast_pn_delivery as pn_cast_pn_delivery,
#     pn_cast_pn_link as pn_cast_pn_link,
#     pn_cast_pn_session as pn_cast_pn_session,
#     pn_cast_pn_transport as pn_cast_pn_transport,
#     pn_collector as pn_collector,
#     pn_collector_free as pn_collector_free,
#     pn_collector_more as pn_collector_more,
#     pn_collector_peek as pn_collector_peek,
#     pn_collector_pop as pn_collector_pop,
#     pn_collector_release as pn_collector_release,
#     pn_condition_clear as pn_condition_clear,
#     pn_condition_info as pn_condition_info,
#     pn_condition_is_set as pn_condition_is_set,
#     pn_connection as pn_connection,
#     pn_connection_attachments as pn_connection_attachments,
#     pn_connection_close as pn_connection_close,
#     pn_connection_collect as pn_connection_collect,
#     pn_connection_condition as pn_connection_condition,
#     pn_connection_desired_capabilities as pn_connection_desired_capabilities,
#     pn_connection_error as pn_connection_error,
#     pn_connection_offered_capabilities as pn_connection_offered_capabilities,
#     pn_connection_open as pn_connection_open,
#     pn_connection_properties as pn_connection_properties,
#     pn_connection_release as pn_connection_release,
#     pn_connection_remote_condition as pn_connection_remote_condition,
#     pn_connection_remote_desired_capabilities as pn_connection_remote_desired_capabilities,
#     pn_connection_remote_offered_capabilities as pn_connection_remote_offered_capabilities,
#     pn_connection_remote_properties as pn_connection_remote_properties,
#     pn_connection_state as pn_connection_state,
#     pn_connection_transport as pn_connection_transport,
#     pn_data as pn_data,
#     pn_data_clear as pn_data_clear,
#     pn_data_copy as pn_data_copy,
#     pn_data_dump as pn_data_dump,
#     pn_data_encoded_size as pn_data_encoded_size,
#     pn_data_enter as pn_data_enter,
#     pn_data_error as pn_data_error,
#     pn_data_exit as pn_data_exit,
#     pn_data_free as pn_data_free,
#     pn_data_get_array as pn_data_get_array,
#     pn_data_get_array_type as pn_data_get_array_type,
#     pn_data_get_bool as pn_data_get_bool,
#     pn_data_get_byte as pn_data_get_byte,
#     pn_data_get_char as pn_data_get_char,
#     pn_data_get_decimal32 as pn_data_get_decimal32,
#     pn_data_get_decimal64 as pn_data_get_decimal64,
#     pn_data_get_double as pn_data_get_double,
#     pn_data_get_float as pn_data_get_float,
#     pn_data_get_int as pn_data_get_int,
#     pn_data_get_list as pn_data_get_list,
#     pn_data_get_long as pn_data_get_long,
#     pn_data_get_map as pn_data_get_map,
#     pn_data_get_short as pn_data_get_short,
#     pn_data_get_timestamp as pn_data_get_timestamp,
#     pn_data_get_ubyte as pn_data_get_ubyte,
#     pn_data_get_uint as pn_data_get_uint,
#     pn_data_get_ulong as pn_data_get_ulong,
#     pn_data_get_ushort as pn_data_get_ushort,
#     pn_data_is_array_described as pn_data_is_array_described,
#     pn_data_is_described as pn_data_is_described,
#     pn_data_is_null as pn_data_is_null,
#     pn_data_narrow as pn_data_narrow,
#     pn_data_next as pn_data_next,
#     pn_data_prev as pn_data_prev,
#     pn_data_put_array as pn_data_put_array,
#     pn_data_put_bool as pn_data_put_bool,
#     pn_data_put_byte as pn_data_put_byte,
#     pn_data_put_char as pn_data_put_char,
#     pn_data_put_decimal32 as pn_data_put_decimal32,
#     pn_data_put_decimal64 as pn_data_put_decimal64,
#     pn_data_put_described as pn_data_put_described,
#     pn_data_put_double as pn_data_put_double,
#     pn_data_put_float as pn_data_put_float,
#     pn_data_put_int as pn_data_put_int,
#     pn_data_put_list as pn_data_put_list,
#     pn_data_put_long as pn_data_put_long,
#     pn_data_put_map as pn_data_put_map,
#     pn_data_put_null as pn_data_put_null,
#     pn_data_put_short as pn_data_put_short,
#     pn_data_put_timestamp as pn_data_put_timestamp,
#     pn_data_put_ubyte as pn_data_put_ubyte,
#     pn_data_put_uint as pn_data_put_uint,
#     pn_data_put_ulong as pn_data_put_ulong,
#     pn_data_put_ushort as pn_data_put_ushort,
#     pn_data_rewind as pn_data_rewind,
#     pn_data_type as pn_data_type,
#     pn_data_widen as pn_data_widen,
#     pn_decref as pn_decref,
#     pn_delivery_abort as pn_delivery_abort,
#     pn_delivery_aborted as pn_delivery_aborted,
#     pn_delivery_attachments as pn_delivery_attachments,
#     pn_delivery_link as pn_delivery_link,
#     pn_delivery_local as pn_delivery_local,
#     pn_delivery_local_state as pn_delivery_local_state,
#     pn_delivery_partial as pn_delivery_partial,
#     pn_delivery_pending as pn_delivery_pending,
#     pn_delivery_readable as pn_delivery_readable,
#     pn_delivery_remote as pn_delivery_remote,
#     pn_delivery_remote_state as pn_delivery_remote_state,
#     pn_delivery_settle as pn_delivery_settle,
#     pn_delivery_settled as pn_delivery_settled,
#     pn_delivery_update as pn_delivery_update,
#     pn_delivery_updated as pn_delivery_updated,
#     pn_delivery_writable as pn_delivery_writable,
#     pn_disposition_annotations as pn_disposition_annotations,
#     pn_disposition_condition as pn_disposition_condition,
#     pn_disposition_data as pn_disposition_data,
#     pn_disposition_get_section_number as pn_disposition_get_section_number,
#     pn_disposition_get_section_offset as pn_disposition_get_section_offset,
#     pn_disposition_is_failed as pn_disposition_is_failed,
#     pn_disposition_is_undeliverable as pn_disposition_is_undeliverable,
#     pn_disposition_set_failed as pn_disposition_set_failed,
#     pn_disposition_set_section_number as pn_disposition_set_section_number,
#     pn_disposition_set_section_offset as pn_disposition_set_section_offset,
#     pn_disposition_set_undeliverable as pn_disposition_set_undeliverable,
#     pn_disposition_type as pn_disposition_type,
#     pn_error_code as pn_error_code,
#     pn_event_connection as pn_event_connection,
#     pn_event_context as pn_event_context,
#     pn_event_delivery as pn_event_delivery,
#     pn_event_link as pn_event_link,
#     pn_event_session as pn_event_session,
#     pn_event_transport as pn_event_transport,
#     pn_event_type as pn_event_type,
#     pn_incref as pn_incref,
#     pn_link_advance as pn_link_advance,
#     pn_link_attachments as pn_link_attachments,
#     pn_link_available as pn_link_available,
#     pn_link_close as pn_link_close,
#     pn_link_condition as pn_link_condition,
#     pn_link_credit as pn_link_credit,
#     pn_link_current as pn_link_current,
#     pn_link_detach as pn_link_detach,
#     pn_link_drain as pn_link_drain,
#     pn_link_drained as pn_link_drained,
#     pn_link_draining as pn_link_draining,
#     pn_link_error as pn_link_error,
#     pn_link_flow as pn_link_flow,
#     pn_link_free as pn_link_free,
#     pn_link_get_drain as pn_link_get_drain,
#     pn_link_head as pn_link_head,
#     pn_link_is_receiver as pn_link_is_receiver,
#     pn_link_is_sender as pn_link_is_sender,
#     pn_link_max_message_size as pn_link_max_message_size,
#     pn_link_next as pn_link_next,
#     pn_link_offered as pn_link_offered,
#     pn_link_open as pn_link_open,
#     pn_link_properties as pn_link_properties,
#     pn_link_queued as pn_link_queued,
#     pn_link_rcv_settle_mode as pn_link_rcv_settle_mode,
#     pn_link_remote_condition as pn_link_remote_condition,
#     pn_link_remote_max_message_size as pn_link_remote_max_message_size,
#     pn_link_remote_properties as pn_link_remote_properties,
#     pn_link_remote_rcv_settle_mode as pn_link_remote_rcv_settle_mode,
#     pn_link_remote_snd_settle_mode as pn_link_remote_snd_settle_mode,
#     pn_link_remote_source as pn_link_remote_source,
#     pn_link_remote_target as pn_link_remote_target,
#     pn_link_session as pn_link_session,
#     pn_link_set_drain as pn_link_set_drain,
#     pn_link_set_max_message_size as pn_link_set_max_message_size,
#     pn_link_set_rcv_settle_mode as pn_link_set_rcv_settle_mode,
#     pn_link_set_snd_settle_mode as pn_link_set_snd_settle_mode,
#     pn_link_snd_settle_mode as pn_link_snd_settle_mode,
#     pn_link_source as pn_link_source,
#     pn_link_state as pn_link_state,
#     pn_link_target as pn_link_target,
#     pn_link_unsettled as pn_link_unsettled,
#     pn_message as pn_message,
#     pn_message_annotations as pn_message_annotations,
#     pn_message_body as pn_message_body,
#     pn_message_clear as pn_message_clear,
#     pn_message_error as pn_message_error,
#     pn_message_free as pn_message_free,
#     pn_message_get_creation_time as pn_message_get_creation_time,
#     pn_message_get_delivery_count as pn_message_get_delivery_count,
#     pn_message_get_expiry_time as pn_message_get_expiry_time,
#     pn_message_get_group_sequence as pn_message_get_group_sequence,
#     pn_message_get_priority as pn_message_get_priority,
#     pn_message_get_ttl as pn_message_get_ttl,
#     pn_message_instructions as pn_message_instructions,
#     pn_message_is_durable as pn_message_is_durable,
#     pn_message_is_first_acquirer as pn_message_is_first_acquirer,
#     pn_message_is_inferred as pn_message_is_inferred,
#     pn_message_properties as pn_message_properties,
#     pn_message_set_creation_time as pn_message_set_creation_time,
#     pn_message_set_delivery_count as pn_message_set_delivery_count,
#     pn_message_set_durable as pn_message_set_durable,
#     pn_message_set_expiry_time as pn_message_set_expiry_time,
#     pn_message_set_first_acquirer as pn_message_set_first_acquirer,
#     pn_message_set_group_sequence as pn_message_set_group_sequence,
#     pn_message_set_inferred as pn_message_set_inferred,
#     pn_message_set_priority as pn_message_set_priority,
#     pn_message_set_ttl as pn_message_set_ttl,
#     pn_sasl as pn_sasl,
#     pn_sasl_done as pn_sasl_done,
#     pn_sasl_extended as pn_sasl_extended,
#     pn_sasl_get_allow_insecure_mechs as pn_sasl_get_allow_insecure_mechs,
#     pn_sasl_outcome as pn_sasl_outcome,
#     pn_sasl_set_allow_insecure_mechs as pn_sasl_set_allow_insecure_mechs,
#     pn_session as pn_session,
#     pn_session_attachments as pn_session_attachments,
#     pn_session_close as pn_session_close,
#     pn_session_condition as pn_session_condition,
#     pn_session_connection as pn_session_connection,
#     pn_session_free as pn_session_free,
#     pn_session_get_incoming_capacity as pn_session_get_incoming_capacity,
#     pn_session_get_outgoing_window as pn_session_get_outgoing_window,
#     pn_session_head as pn_session_head,
#     pn_session_incoming_bytes as pn_session_incoming_bytes,
#     pn_session_next as pn_session_next,
#     pn_session_open as pn_session_open,
#     pn_session_outgoing_bytes as pn_session_outgoing_bytes,
#     pn_session_remote_condition as pn_session_remote_condition,
#     pn_session_set_incoming_capacity as pn_session_set_incoming_capacity,
#     pn_session_set_outgoing_window as pn_session_set_outgoing_window,
#     pn_session_state as pn_session_state,
#     pn_ssl as pn_ssl,
#     pn_ssl_domain as pn_ssl_domain,
#     pn_ssl_domain_allow_unsecured_client as pn_ssl_domain_allow_unsecured_client,
#     pn_ssl_domain_free as pn_ssl_domain_free,
#     pn_ssl_present as pn_ssl_present,
#     pn_ssl_resume_status as pn_ssl_resume_status,
#     pn_terminus_capabilities as pn_terminus_capabilities,
#     pn_terminus_copy as pn_terminus_copy,
#     pn_terminus_filter as pn_terminus_filter,
#     pn_terminus_get_distribution_mode as pn_terminus_get_distribution_mode,
#     pn_terminus_get_durability as pn_terminus_get_durability,
#     pn_terminus_get_expiry_policy as pn_terminus_get_expiry_policy,
#     pn_terminus_get_timeout as pn_terminus_get_timeout,
#     pn_terminus_get_type as pn_terminus_get_type,
#     pn_terminus_is_dynamic as pn_terminus_is_dynamic,
#     pn_terminus_outcomes as pn_terminus_outcomes,
#     pn_terminus_properties as pn_terminus_properties,
#     pn_terminus_set_distribution_mode as pn_terminus_set_distribution_mode,
#     pn_terminus_set_durability as pn_terminus_set_durability,
#     pn_terminus_set_dynamic as pn_terminus_set_dynamic,
#     pn_terminus_set_expiry_policy as pn_terminus_set_expiry_policy,
#     pn_terminus_set_timeout as pn_terminus_set_timeout,
#     pn_terminus_set_type as pn_terminus_set_type,
#     pn_transport as pn_transport,
#     pn_transport_attachments as pn_transport_attachments,
#     pn_transport_bind as pn_transport_bind,
#     pn_transport_capacity as pn_transport_capacity,
#     pn_transport_close_head as pn_transport_close_head,
#     pn_transport_close_tail as pn_transport_close_tail,
#     pn_transport_closed as pn_transport_closed,
#     pn_transport_condition as pn_transport_condition,
#     pn_transport_connection as pn_transport_connection,
#     pn_transport_error as pn_transport_error,
#     pn_transport_get_channel_max as pn_transport_get_channel_max,
#     pn_transport_get_frames_input as pn_transport_get_frames_input,
#     pn_transport_get_frames_output as pn_transport_get_frames_output,
#     pn_transport_get_idle_timeout as pn_transport_get_idle_timeout,
#     pn_transport_get_max_frame as pn_transport_get_max_frame,
#     pn_transport_get_remote_idle_timeout as pn_transport_get_remote_idle_timeout,
#     pn_transport_get_remote_max_frame as pn_transport_get_remote_max_frame,
#     pn_transport_is_authenticated as pn_transport_is_authenticated,
#     pn_transport_is_encrypted as pn_transport_is_encrypted,
#     pn_transport_pending as pn_transport_pending,
#     pn_transport_pop as pn_transport_pop,
#     pn_transport_remote_channel_max as pn_transport_remote_channel_max,
#     pn_transport_require_auth as pn_transport_require_auth,
#     pn_transport_require_encryption as pn_transport_require_encryption,
#     pn_transport_set_channel_max as pn_transport_set_channel_max,
#     pn_transport_set_idle_timeout as pn_transport_set_idle_timeout,
#     pn_transport_set_max_frame as pn_transport_set_max_frame,
#     pn_transport_set_server as pn_transport_set_server,
#     pn_transport_tick as pn_transport_tick,
#     pn_transport_trace as pn_transport_trace,
#     pn_transport_unbind as pn_transport_unbind,
# )

CData: TypeAlias = ffi.CData

# typedef struct pn_bytes_t;
pn_bytes_t = NewType('pn_bytes_t', CData)

# typedef uint32_t pn_char_t;
pn_char_t = int

# typedef struct pn_collector_t pn_collector_t;
pn_collector_t = NewType('pn_collector_t', CData)

# typedef struct pn_condition_t pn_condition_t;
pn_condition_t = NewType('pn_condition_t', CData)

# typedef struct pn_connection_t pn_connection_t;
pn_connection_t = NewType('pn_connection_t', CData)

# typedef struct pn_data_t pn_data_t;
pn_data_t = NewType('pn_data_t', CData)

# typedef struct pn_decimal128_t;
pn_decimal128_t = NewType('pn_decimal128_t', CData)

# typedef uint32_t pn_decimal32_t;
pn_decimal32_t = int

# typedef uint64_t pn_decimal64_t;
pn_decimal64_t = int

# typedef struct pn_delivery_t pn_delivery_t;
pn_delivery_t = NewType('pn_delivery_t', CData)

# typedef pn_bytes_t pn_delivery_tag_t;
pn_delivery_tag_t = pn_bytes_t

# typedef struct pn_disposition_t pn_disposition_t;
pn_disposition_t = NewType('pn_disposition_t', CData)

# typedef enum pn_distribution_mode_t;
pn_distribution_mode_t = int

# typedef enum pn_durability_t;
pn_durability_t = int

# typedef struct pn_error_t pn_error_t;
pn_error_t = NewType('pn_error_t', CData)

# typedef struct pn_event_t pn_event_t;
pn_event_t = NewType('pn_event_t', CData)

# typedef enum pn_event_type_t;
pn_event_type_t = int

# typedef enum pn_expiry_policy_t;
pn_expiry_policy_t = int

# typedef struct pn_link_t pn_link_t;
pn_link_t = NewType('pn_link_t', CData)

# typedef struct pn_message_t pn_message_t;
pn_message_t = NewType('pn_message_t', CData)

# typedef uint32_t pn_millis_t;
pn_millis_t = int

# typedef enum pn_rcv_settle_mode_t;
pn_rcv_settle_mode_t = int

# typedef struct pn_record_t pn_record_t;
pn_record_t = NewType('pn_record_t', CData)

# typedef enum pn_sasl_outcome_t;
pn_sasl_outcome_t = int

# typedef struct pn_sasl_t pn_sasl_t;
pn_sasl_t = NewType('pn_sasl_t', CData)

# typedef uint32_t pn_seconds_t;
pn_seconds_t = int

# typedef uint32_t pn_sequence_t;
pn_sequence_t = int

# typedef struct pn_session_t pn_session_t;
pn_session_t = NewType('pn_session_t', CData)

# typedef enum pn_snd_settle_mode_t;
pn_snd_settle_mode_t = int

# typedef enum pn_ssl_cert_subject_subfield;
pn_ssl_cert_subject_subfield = int

# typedef struct pn_ssl_domain_t pn_ssl_domain_t;
pn_ssl_domain_t = NewType('pn_ssl_domain_t', CData)

# typedef enum pn_ssl_hash_alg;
pn_ssl_hash_alg = int

# typedef enum pn_ssl_mode_t;
pn_ssl_mode_t = int

# typedef enum pn_ssl_resume_status_t;
pn_ssl_resume_status_t = int

# typedef struct pn_ssl_t pn_ssl_t;
pn_ssl_t = NewType('pn_ssl_t', CData)

# typedef enum pn_ssl_verify_mode_t;
pn_ssl_verify_mode_t = int

# typedef int pn_state_t;
pn_state_t = int

# typedef struct pn_terminus_t pn_terminus_t;
pn_terminus_t = NewType('pn_terminus_t', CData)

# typedef enum pn_terminus_type_t;
pn_terminus_type_t = int

# typedef int64_t pn_timestamp_t;
pn_timestamp_t = int

# typedef int pn_trace_t;
pn_trace_t = int

# typedef struct pn_transport_t pn_transport_t;
pn_transport_t = NewType('pn_transport_t', CData)

# typedef enum pn_type_t;
pn_type_t = int

# typedef struct pn_uuid_t;
pn_uuid_t = NewType('pn_uuid_t', CData)

# typedef struct pn_atom_t;
pn_atom_t = NewType('pn_atom_t', CData)

# typedef pn_atom_t pn_msgid_t;
pn_msgid_t = pn_atom_t

PN_DIST_MODE_UNSPECIFIED: int
PN_DIST_MODE_COPY: int
PN_DIST_MODE_MOVE: int
PN_NONDURABLE: int
PN_CONFIGURATION: int
PN_DELIVERIES: int
PN_EVENT_NONE: int
PN_REACTOR_INIT: int
PN_REACTOR_QUIESCED: int
PN_REACTOR_FINAL: int
PN_TIMER_TASK: int
PN_CONNECTION_INIT: int
PN_CONNECTION_BOUND: int
PN_CONNECTION_UNBOUND: int
PN_CONNECTION_LOCAL_OPEN: int
PN_CONNECTION_REMOTE_OPEN: int
PN_CONNECTION_LOCAL_CLOSE: int
PN_CONNECTION_REMOTE_CLOSE: int
PN_CONNECTION_FINAL: int
PN_SESSION_INIT: int
PN_SESSION_LOCAL_OPEN: int
PN_SESSION_REMOTE_OPEN: int
PN_SESSION_LOCAL_CLOSE: int
PN_SESSION_REMOTE_CLOSE: int
PN_SESSION_FINAL: int
PN_LINK_INIT: int
PN_LINK_LOCAL_OPEN: int
PN_LINK_REMOTE_OPEN: int
PN_LINK_LOCAL_CLOSE: int
PN_LINK_REMOTE_CLOSE: int
PN_LINK_LOCAL_DETACH: int
PN_LINK_REMOTE_DETACH: int
PN_LINK_FLOW: int
PN_LINK_FINAL: int
PN_DELIVERY: int
PN_TRANSPORT: int
PN_TRANSPORT_AUTHENTICATED: int
PN_TRANSPORT_ERROR: int
PN_TRANSPORT_HEAD_CLOSED: int
PN_TRANSPORT_TAIL_CLOSED: int
PN_TRANSPORT_CLOSED: int
PN_SELECTABLE_INIT: int
PN_SELECTABLE_UPDATED: int
PN_SELECTABLE_READABLE: int
PN_SELECTABLE_WRITABLE: int
PN_SELECTABLE_ERROR: int
PN_SELECTABLE_EXPIRED: int
PN_SELECTABLE_FINAL: int
PN_CONNECTION_WAKE: int
PN_LISTENER_ACCEPT: int
PN_LISTENER_CLOSE: int
PN_PROACTOR_INTERRUPT: int
PN_PROACTOR_TIMEOUT: int
PN_PROACTOR_INACTIVE: int
PN_LISTENER_OPEN: int
PN_RAW_CONNECTION_CONNECTED: int
PN_RAW_CONNECTION_CLOSED_READ: int
PN_RAW_CONNECTION_CLOSED_WRITE: int
PN_RAW_CONNECTION_DISCONNECTED: int
PN_RAW_CONNECTION_NEED_READ_BUFFERS: int
PN_RAW_CONNECTION_NEED_WRITE_BUFFERS: int
PN_RAW_CONNECTION_READ: int
PN_RAW_CONNECTION_WRITTEN: int
PN_RAW_CONNECTION_WAKE: int
PN_RAW_CONNECTION_DRAIN_BUFFERS: int
PN_EXPIRE_WITH_LINK: int
PN_EXPIRE_WITH_SESSION: int
PN_EXPIRE_WITH_CONNECTION: int
PN_EXPIRE_NEVER: int
PN_RCV_FIRST: int
PN_RCV_SECOND: int
PN_SASL_NONE: int
PN_SASL_OK: int
PN_SASL_AUTH: int
PN_SASL_SYS: int
PN_SASL_PERM: int
PN_SASL_TEMP: int
PN_SND_UNSETTLED: int
PN_SND_SETTLED: int
PN_SND_MIXED: int
PN_SSL_CERT_SUBJECT_COUNTRY_NAME: int
PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE: int
PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY: int
PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME: int
PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT: int
PN_SSL_CERT_SUBJECT_COMMON_NAME: int
PN_SSL_SHA1: int
PN_SSL_SHA256: int
PN_SSL_SHA512: int
PN_SSL_MD5: int
PN_SSL_MODE_CLIENT: int
PN_SSL_MODE_SERVER: int
PN_SSL_RESUME_UNKNOWN: int
PN_SSL_RESUME_NEW: int
PN_SSL_RESUME_REUSED: int
PN_SSL_VERIFY_NULL: int
PN_SSL_VERIFY_PEER: int
PN_SSL_ANONYMOUS_PEER: int
PN_SSL_VERIFY_PEER_NAME: int
PN_UNSPECIFIED: int
PN_SOURCE: int
PN_TARGET: int
PN_COORDINATOR: int
PN_NULL: int
PN_BOOL: int
PN_UBYTE: int
PN_BYTE: int
PN_USHORT: int
PN_SHORT: int
PN_UINT: int
PN_INT: int
PN_CHAR: int
PN_ULONG: int
PN_LONG: int
PN_TIMESTAMP: int
PN_FLOAT: int
PN_DOUBLE: int
PN_DECIMAL32: int
PN_DECIMAL64: int
PN_DECIMAL128: int
PN_UUID: int
PN_BINARY: int
PN_STRING: int
PN_SYMBOL: int
PN_DESCRIBED: int
PN_ARRAY: int
PN_LIST: int
PN_MAP: int
PN_INVALID: int


# pn_collector_t *pn_collector(void);
def pn_collector() -> pn_collector_t: ...
# void pn_collector_free(pn_collector_t *collector);
def pn_collector_free(collector: pn_collector_t) -> None: ...
# _Bool pn_collector_more(pn_collector_t *collector);
def pn_collector_more(collector: pn_collector_t) -> bool: ...
# pn_event_t *pn_collector_peek(pn_collector_t *collector);
def pn_collector_peek(collector: pn_collector_t) -> pn_event_t: ...
# _Bool pn_collector_pop(pn_collector_t *collector);
def pn_collector_pop(collector: pn_collector_t) -> bool: ...
# void pn_collector_release(pn_collector_t *collector);
def pn_collector_release(collector: pn_collector_t) -> None: ...


# void pn_condition_clear(pn_condition_t *condition);
def pn_condition_clear(condition: pn_condition_t) -> None: ...
# const char *pn_condition_get_description(pn_condition_t *condition);
def pn_condition_get_description(condition: pn_condition_t) -> str: ...
# const char *pn_condition_get_name(pn_condition_t *condition);
def pn_condition_get_name(condition: pn_condition_t) -> str: ...
# pn_data_t *pn_condition_info(pn_condition_t *condition);
def pn_condition_info(condition: pn_condition_t) -> pn_data_t: ...
# _Bool pn_condition_is_set(pn_condition_t *condition);
def pn_condition_is_set(condition: pn_condition_t) -> bool: ...
# int pn_condition_set_description(pn_condition_t *condition, const char *description);
def pn_condition_set_description(condition: pn_condition_t, description: str) -> int: ...
# int pn_condition_set_name(pn_condition_t *condition, const char *name);
def pn_condition_set_name(condition: pn_condition_t, name: str) -> int: ...


# pn_connection_t *pn_connection(void);
def pn_connection() -> pn_connection_t: ...
# pn_record_t *pn_connection_attachments(pn_connection_t *connection);
def pn_connection_attachments(connection: pn_connection_t) -> pn_record_t: ...
# void pn_connection_close(pn_connection_t *connection);
def pn_connection_close(connection: pn_connection_t) -> None: ...
# void pn_connection_collect(pn_connection_t *connection, pn_collector_t *collector);
def pn_connection_collect(connection: pn_connection_t, collector: pn_collector_t) -> None: ...
# pn_condition_t *pn_connection_condition(pn_connection_t *connection);
def pn_connection_condition(connection: pn_connection_t) -> pn_condition_t: ...
# pn_data_t *pn_connection_desired_capabilities(pn_connection_t *connection);
def pn_connection_desired_capabilities(connection: pn_connection_t) -> pn_data_t: ...
# pn_error_t *pn_connection_error(pn_connection_t *connection);
def pn_connection_error(connection: pn_connection_t) -> pn_error_t: ...
# const char *pn_connection_get_authorization(pn_connection_t *connection);
def pn_connection_get_authorization(connection: pn_connection_t) -> str: ...
# const char *pn_connection_get_container(pn_connection_t *connection);
def pn_connection_get_container(connection: pn_connection_t) -> str: ...
# const char *pn_connection_get_hostname(pn_connection_t *connection);
def pn_connection_get_hostname(connection: pn_connection_t) -> str: ...
# const char *pn_connection_get_user(pn_connection_t *connection);
def pn_connection_get_user(connection: pn_connection_t) -> str: ...
# pn_data_t *pn_connection_offered_capabilities(pn_connection_t *connection);
def pn_connection_offered_capabilities(connection: pn_connection_t) -> pn_data_t: ...
# void pn_connection_open(pn_connection_t *connection);
def pn_connection_open(connection: pn_connection_t) -> None: ...
# pn_data_t *pn_connection_properties(pn_connection_t *connection);
def pn_connection_properties(connection: pn_connection_t) -> pn_data_t: ...
# void pn_connection_release(pn_connection_t *connection);
def pn_connection_release(connection: pn_connection_t) -> None: ...
# pn_condition_t *pn_connection_remote_condition(pn_connection_t *connection);
def pn_connection_remote_condition(connection: pn_connection_t) -> pn_condition_t: ...
# const char *pn_connection_remote_container(pn_connection_t *connection);
def pn_connection_remote_container(connection: pn_connection_t) -> str: ...
# pn_data_t *pn_connection_remote_desired_capabilities(pn_connection_t *connection);
def pn_connection_remote_desired_capabilities(connection: pn_connection_t) -> pn_data_t: ...
# const char *pn_connection_remote_hostname(pn_connection_t *connection);
def pn_connection_remote_hostname(connection: pn_connection_t) -> str: ...
# pn_data_t *pn_connection_remote_offered_capabilities(pn_connection_t *connection);
def pn_connection_remote_offered_capabilities(connection: pn_connection_t) -> pn_data_t: ...
# pn_data_t *pn_connection_remote_properties(pn_connection_t *connection);
def pn_connection_remote_properties(connection: pn_connection_t) -> pn_data_t: ...
# void pn_connection_set_authorization(pn_connection_t *connection, const char *authzid);
def pn_connection_set_authorization(connection: pn_connection_t, authzid: str) -> None: ...
# void pn_connection_set_container(pn_connection_t *connection, const char *container);
def pn_connection_set_container(connection: pn_connection_t, container: str) -> None: ...
# void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname);
def pn_connection_set_hostname(connection: pn_connection_t, hostname: str) -> None: ...
# void pn_connection_set_password(pn_connection_t *connection, const char *password);
def pn_connection_set_password(connection: pn_connection_t, password: str) -> None: ...
# void pn_connection_set_user(pn_connection_t *connection, const char *user);
def pn_connection_set_user(connection: pn_connection_t, user: str) -> None: ...
# pn_state_t pn_connection_state(pn_connection_t *connection);
def pn_connection_state(connection: pn_connection_t) -> pn_state_t: ...
# pn_transport_t *pn_connection_transport(pn_connection_t *connection);
def pn_connection_transport(connection: pn_connection_t) -> pn_transport_t: ...


# pn_data_t *pn_data(size_t capacity);
def pn_data(capacity: int) -> pn_data_t: ...
# void pn_data_clear(pn_data_t *data);
def pn_data_clear(data: pn_data_t) -> None: ...
# int pn_data_copy(pn_data_t *data, pn_data_t *src);
def pn_data_copy(data: pn_data_t, src: pn_data_t) -> int: ...
# ssize_t pn_data_decode(pn_data_t *data, const char *bytes, size_t size);
def pn_data_decode(data: pn_data_t, bytes: str, size: int) -> int: ...
# void pn_data_dump(pn_data_t *data);
def pn_data_dump(data: pn_data_t) -> None: ...
# ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size);
def pn_data_encode(data: pn_data_t, bytes: str, size: int) -> int: ...
# ssize_t pn_data_encoded_size(pn_data_t *data);
def pn_data_encoded_size(data: pn_data_t) -> int: ...
# _Bool pn_data_enter(pn_data_t *data);
def pn_data_enter(data: pn_data_t) -> bool: ...
# pn_error_t *pn_data_error(pn_data_t *data);
def pn_data_error(data: pn_data_t) -> pn_error_t: ...
# _Bool pn_data_exit(pn_data_t *data);
def pn_data_exit(data: pn_data_t) -> bool: ...
# void pn_data_free(pn_data_t *data);
def pn_data_free(data: pn_data_t) -> None: ...
# size_t pn_data_get_array(pn_data_t *data);
def pn_data_get_array(data: pn_data_t) -> int: ...
# pn_type_t pn_data_get_array_type(pn_data_t *data);
def pn_data_get_array_type(data: pn_data_t) -> pn_type_t: ...
# pn_bytes_t pn_data_get_binary(pn_data_t *data);
def pn_data_get_binary(data: pn_data_t) -> pn_bytes_t: ...
# _Bool pn_data_get_bool(pn_data_t *data);
def pn_data_get_bool(data: pn_data_t) -> bool: ...
# int8_t pn_data_get_byte(pn_data_t *data);
def pn_data_get_byte(data: pn_data_t) -> int: ...
# pn_char_t pn_data_get_char(pn_data_t *data);
def pn_data_get_char(data: pn_data_t) -> pn_char_t: ...
# pn_decimal128_t pn_data_get_decimal128(pn_data_t *data);
def pn_data_get_decimal128(data: pn_data_t) -> pn_decimal128_t: ...
# pn_decimal32_t pn_data_get_decimal32(pn_data_t *data);
def pn_data_get_decimal32(data: pn_data_t) -> pn_decimal32_t: ...
# pn_decimal64_t pn_data_get_decimal64(pn_data_t *data);
def pn_data_get_decimal64(data: pn_data_t) -> pn_decimal64_t: ...
# double pn_data_get_double(pn_data_t *data);
def pn_data_get_double(data: pn_data_t) -> float: ...
# float pn_data_get_float(pn_data_t *data);
def pn_data_get_float(data: pn_data_t) -> float: ...
# int32_t pn_data_get_int(pn_data_t *data);
def pn_data_get_int(data: pn_data_t) -> int: ...
# size_t pn_data_get_list(pn_data_t *data);
def pn_data_get_list(data: pn_data_t) -> int: ...
# int64_t pn_data_get_long(pn_data_t *data);
def pn_data_get_long(data: pn_data_t) -> int: ...
# size_t pn_data_get_map(pn_data_t *data);
def pn_data_get_map(data: pn_data_t) -> int: ...
# int16_t pn_data_get_short(pn_data_t *data);
def pn_data_get_short(data: pn_data_t) -> int: ...
# pn_bytes_t pn_data_get_string(pn_data_t *data);
def pn_data_get_string(data: pn_data_t) -> pn_bytes_t: ...
# pn_bytes_t pn_data_get_symbol(pn_data_t *data);
def pn_data_get_symbol(data: pn_data_t) -> pn_bytes_t: ...
# pn_timestamp_t pn_data_get_timestamp(pn_data_t *data);
def pn_data_get_timestamp(data: pn_data_t) -> int: ...
# uint8_t pn_data_get_ubyte(pn_data_t *data);
def pn_data_get_ubyte(data: pn_data_t) -> int: ...
# uint32_t pn_data_get_uint(pn_data_t *data);
def pn_data_get_uint(data: pn_data_t) -> int: ...
# uint64_t pn_data_get_ulong(pn_data_t *data);
def pn_data_get_ulong(data: pn_data_t) -> int: ...
# uint16_t pn_data_get_ushort(pn_data_t *data);
def pn_data_get_ushort(data: pn_data_t) -> int: ...
# pn_uuid_t pn_data_get_uuid(pn_data_t *data);
def pn_data_get_uuid(data: pn_data_t) -> pn_uuid_t: ...
# _Bool pn_data_is_array_described(pn_data_t *data);
def pn_data_is_array_described(data: pn_data_t) -> bool: ...
# _Bool pn_data_is_described(pn_data_t *data);
def pn_data_is_described(data: pn_data_t) -> bool: ...
# _Bool pn_data_is_null(pn_data_t *data);
def pn_data_is_null(data: pn_data_t) -> bool: ...
# _Bool pn_data_lookup(pn_data_t *data, const char *name);
def pn_data_lookup(data: pn_data_t, name: str) -> bool: ...
# void pn_data_narrow(pn_data_t *data);
def pn_data_narrow(data: pn_data_t) -> None: ...
# _Bool pn_data_next(pn_data_t *data);
def pn_data_next(data: pn_data_t) -> bool: ...
# _Bool pn_data_prev(pn_data_t *data);
def pn_data_prev(data: pn_data_t) -> bool: ...
# int pn_data_put_array(pn_data_t *data, _Bool described, pn_type_t type);
def pn_data_put_array(data: pn_data_t, described: bool, type: pn_type_t) -> int: ...
# int pn_data_put_binary(pn_data_t *data, pn_bytes_t bytes);
def pn_data_put_binary(data: pn_data_t, bytes: pn_bytes_t) -> int: ...
# int pn_data_put_bool(pn_data_t *data, _Bool b);
def pn_data_put_bool(data: pn_data_t, b: bool) -> int: ...
# int pn_data_put_byte(pn_data_t *data, int8_t b);
def pn_data_put_byte(data: pn_data_t, b: int) -> int: ...
# int pn_data_put_char(pn_data_t *data, pn_char_t c);
def pn_data_put_char(data: pn_data_t, c: pn_char_t) -> int: ...
# int pn_data_put_decimal128(pn_data_t *data, pn_decimal128_t d);
def pn_data_put_decimal128(data: pn_data_t, d: pn_decimal128_t) -> int: ...
# int pn_data_put_decimal32(pn_data_t *data, pn_decimal32_t d);
def pn_data_put_decimal32(data: pn_data_t, d: pn_decimal32_t) -> int: ...
# int pn_data_put_decimal64(pn_data_t *data, pn_decimal64_t d);
def pn_data_put_decimal64(data: pn_data_t, d: pn_decimal64_t) -> int: ...
# int pn_data_put_described(pn_data_t *data);
def pn_data_put_described(data: pn_data_t) -> int: ...
# int pn_data_put_double(pn_data_t *data, double d);
def pn_data_put_double(data: pn_data_t, d: float) -> int: ...
# int pn_data_put_float(pn_data_t *data, float f);
def pn_data_put_float(data: pn_data_t, f: float) -> int: ...
# int pn_data_put_int(pn_data_t *data, int32_t i);
def pn_data_put_int(data: pn_data_t, i: int) -> int: ...
# int pn_data_put_list(pn_data_t *data);
def pn_data_put_list(data: pn_data_t) -> int: ...
# int pn_data_put_long(pn_data_t *data, int64_t value);
def pn_data_put_long(data: pn_data_t, value: int) -> int: ...
# int pn_data_put_map(pn_data_t *data);
def pn_data_put_map(data: pn_data_t) -> int: ...
# int pn_data_put_null(pn_data_t *data);
def pn_data_put_null(data: pn_data_t) -> int: ...
# int pn_data_put_short(pn_data_t *data, int16_t s);
def pn_data_put_short(data: pn_data_t, s: int) -> int: ...
# int pn_data_put_string(pn_data_t *data, pn_bytes_t string);
def pn_data_put_string(data: pn_data_t, string: pn_bytes_t) -> int: ...
# int pn_data_put_symbol(pn_data_t *data, pn_bytes_t symbol);
def pn_data_put_symbol(data: pn_data_t, symbol: pn_bytes_t) -> int: ...
# int pn_data_put_timestamp(pn_data_t *data, pn_timestamp_t t);
def pn_data_put_timestamp(data: pn_data_t, t: int) -> int: ...
# int pn_data_put_ubyte(pn_data_t *data, uint8_t ub);
def pn_data_put_ubyte(data: pn_data_t, ub: int) -> int: ...
# int pn_data_put_uint(pn_data_t *data, uint32_t ui);
def pn_data_put_uint(data: pn_data_t, ui: int) -> int: ...
# int pn_data_put_ulong(pn_data_t *data, uint64_t ul);
def pn_data_put_ulong(data: pn_data_t, ul: int) -> int: ...
# int pn_data_put_ushort(pn_data_t *data, uint16_t us);
def pn_data_put_ushort(data: pn_data_t, us: int) -> int: ...
# int pn_data_put_uuid(pn_data_t *data, pn_uuid_t u);
def pn_data_put_uuid(data: pn_data_t, u: pn_uuid_t) -> int: ...
# void pn_data_rewind(pn_data_t *data);
def pn_data_rewind(data: pn_data_t) -> None: ...
# pn_type_t pn_data_type(pn_data_t *data);
def pn_data_type(data: pn_data_t) -> pn_type_t: ...
# void pn_data_widen(pn_data_t *data);
def pn_data_widen(data: pn_data_t) -> None: ...


# int pn_decref(void *object);
def pn_decref(object: CData) -> int: ...
# char *pn_tostring(void *object);
def pn_tostring(object: CData) -> str: ...


# pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag);
def pn_delivery(link: pn_link_t, tag: pn_delivery_tag_t) -> pn_delivery_t: ...
# void pn_delivery_abort(pn_delivery_t *delivery);
def pn_delivery_abort(delivery: pn_delivery_t) -> None: ...
# _Bool pn_delivery_aborted(pn_delivery_t *delivery);
def pn_delivery_aborted(delivery: pn_delivery_t) -> bool: ...
# pn_record_t *pn_delivery_attachments(pn_delivery_t *delivery);
def pn_delivery_attachments(delivery: pn_delivery_t) -> pn_record_t: ...
# pn_link_t *pn_delivery_link(pn_delivery_t *delivery);
def pn_delivery_link(delivery: pn_delivery_t) -> pn_link_t: ...
# pn_disposition_t *pn_delivery_local(pn_delivery_t *delivery);
def pn_delivery_local(delivery: pn_delivery_t) -> pn_disposition_t: ...
# uint64_t pn_delivery_local_state(pn_delivery_t *delivery);
def pn_delivery_local_state(delivery: pn_delivery_t) -> int: ...
# _Bool pn_delivery_partial(pn_delivery_t *delivery);
def pn_delivery_partial(delivery: pn_delivery_t) -> bool: ...
# size_t pn_delivery_pending(pn_delivery_t *delivery);
def pn_delivery_pending(delivery: pn_delivery_t) -> int: ...
# _Bool pn_delivery_readable(pn_delivery_t *delivery);
def pn_delivery_readable(delivery: pn_delivery_t) -> bool: ...
# pn_disposition_t *pn_delivery_remote(pn_delivery_t *delivery);
def pn_delivery_remote(delivery: pn_delivery_t) -> pn_disposition_t: ...
# uint64_t pn_delivery_remote_state(pn_delivery_t *delivery);
def pn_delivery_remote_state(delivery: pn_delivery_t) -> int: ...
# void pn_delivery_settle(pn_delivery_t *delivery);
def pn_delivery_settle(delivery: pn_delivery_t) -> None: ...
# _Bool pn_delivery_settled(pn_delivery_t *delivery);
def pn_delivery_settled(delivery: pn_delivery_t) -> bool: ...
# pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery);
def pn_delivery_tag(delivery: pn_delivery_t) -> pn_delivery_tag_t: ...
# void pn_delivery_update(pn_delivery_t *delivery, uint64_t state);
def pn_delivery_update(delivery: pn_delivery_t, state: int) -> None: ...
# _Bool pn_delivery_updated(pn_delivery_t *delivery);
def pn_delivery_updated(delivery: pn_delivery_t) -> bool: ...
# _Bool pn_delivery_writable(pn_delivery_t *delivery);
def pn_delivery_writable(delivery: pn_delivery_t) -> bool: ...


# pn_data_t *pn_disposition_annotations(pn_disposition_t *disposition);
def pn_disposition_annotations(disposition: pn_disposition_t) -> pn_data_t: ...
# pn_condition_t *pn_disposition_condition(pn_disposition_t *disposition);
def pn_disposition_condition(disposition: pn_disposition_t) -> pn_condition_t: ...
# pn_data_t *pn_disposition_data(pn_disposition_t *disposition);
def pn_disposition_data(disposition: pn_disposition_t) -> pn_data_t: ...
# uint32_t pn_disposition_get_section_number(pn_disposition_t *disposition);
def pn_disposition_get_section_number(disposition: pn_disposition_t) -> int: ...
# uint64_t pn_disposition_get_section_offset(pn_disposition_t *disposition);
def pn_disposition_get_section_offset(disposition: pn_disposition_t) -> int: ...
# _Bool pn_disposition_is_failed(pn_disposition_t *disposition);
def pn_disposition_is_failed(disposition: pn_disposition_t) -> bool: ...
# _Bool pn_disposition_is_undeliverable(pn_disposition_t *disposition);
def pn_disposition_is_undeliverable(disposition: pn_disposition_t) -> bool: ...
# void pn_disposition_set_failed(pn_disposition_t *disposition, _Bool failed);
def pn_disposition_set_failed(disposition: pn_disposition_t, failed: bool) -> None: ...
# void pn_disposition_set_section_number(pn_disposition_t *disposition, uint32_t section_number);
def pn_disposition_set_section_number(disposition: pn_disposition_t, section_number: int) -> None: ...
# void pn_disposition_set_section_offset(pn_disposition_t *disposition, uint64_t section_offset);
def pn_disposition_set_section_offset(disposition: pn_disposition_t, section_offset: int) -> None: ...
# void pn_disposition_set_undeliverable(pn_disposition_t *disposition, _Bool undeliverable);
def pn_disposition_set_undeliverable(disposition: pn_disposition_t, undeliverable: bool) -> None: ...
# uint64_t pn_disposition_type(pn_disposition_t *disposition);
def pn_disposition_type(disposition: pn_disposition_t) -> int: ...


# int pn_error_code(pn_error_t *error);
def pn_error_code(error: pn_error_t) -> int: ...
# const char *pn_error_text(pn_error_t *error);
def pn_error_text(error: pn_error_t) -> str: ...


# pn_connection_t *pn_event_connection(pn_event_t *event);
def pn_event_connection(event: pn_event_t) -> pn_connection_t: ...
# void *pn_event_context(pn_event_t *event);
def pn_event_context(event: pn_event_t) -> CData: ...
# pn_delivery_t *pn_event_delivery(pn_event_t *event);
def pn_event_delivery(event: pn_event_t) -> pn_delivery_t: ...
# pn_link_t *pn_event_link(pn_event_t *event);
def pn_event_link(event: pn_event_t) -> pn_link_t: ...
# pn_session_t *pn_event_session(pn_event_t *event);
def pn_event_session(event: pn_event_t) -> pn_session_t: ...
# pn_transport_t *pn_event_transport(pn_event_t *event);
def pn_event_transport(event: pn_event_t) -> pn_transport_t: ...
# pn_event_type_t pn_event_type(pn_event_t *event);
def pn_event_type(event: pn_event_t) -> pn_event_type_t: ...
# const char *pn_event_type_name(pn_event_type_t type);
def pn_event_type_name(type: pn_event_type_t) -> str: ...


# void *pn_incref(void *object);
def pn_incref(object: CData) -> CData: ...


# _Bool pn_link_advance(pn_link_t *link);
def pn_link_advance(link: pn_link_t) -> bool: ...
# pn_record_t *pn_link_attachments(pn_link_t *link);
def pn_link_attachments(link: pn_link_t) -> pn_record_t: ...
# int pn_link_available(pn_link_t *link);
def pn_link_available(link: pn_link_t) -> int: ...
# void pn_link_close(pn_link_t *link);
def pn_link_close(link: pn_link_t) -> None: ...
# pn_condition_t *pn_link_condition(pn_link_t *link);
def pn_link_condition(link: pn_link_t) -> pn_condition_t: ...
# int pn_link_credit(pn_link_t *link);
def pn_link_credit(link: pn_link_t) -> int: ...
# pn_delivery_t *pn_link_current(pn_link_t *link);
def pn_link_current(link: pn_link_t) -> pn_delivery_t: ...
# void pn_link_detach(pn_link_t *link);
def pn_link_detach(link: pn_link_t) -> None: ...
# void pn_link_drain(pn_link_t *receiver, int credit);
def pn_link_drain(receiver: pn_link_t, credit: int) -> None: ...
# int pn_link_drained(pn_link_t *link);
def pn_link_drained(link: pn_link_t) -> int: ...
# _Bool pn_link_draining(pn_link_t *receiver);
def pn_link_draining(receiver: pn_link_t) -> bool: ...
# pn_error_t *pn_link_error(pn_link_t *link);
def pn_link_error(link: pn_link_t) -> pn_error_t: ...
# void pn_link_flow(pn_link_t *receiver, int credit);
def pn_link_flow(receiver: pn_link_t, credit: int) -> None: ...
# void pn_link_free(pn_link_t *link);
def pn_link_free(link: pn_link_t) -> None: ...
# _Bool pn_link_get_drain(pn_link_t *link);
def pn_link_get_drain(link: pn_link_t) -> bool: ...
# pn_link_t *pn_link_head(pn_connection_t *connection, pn_state_t state);
def pn_link_head(connection: pn_connection_t, state: pn_state_t) -> pn_link_t: ...
# _Bool pn_link_is_receiver(pn_link_t *link);
def pn_link_is_receiver(link: pn_link_t) -> bool: ...
# _Bool pn_link_is_sender(pn_link_t *link);
def pn_link_is_sender(link: pn_link_t) -> bool: ...
# uint64_t pn_link_max_message_size(pn_link_t *link);
def pn_link_max_message_size(link: pn_link_t) -> int: ...
# const char *pn_link_name(pn_link_t *link);
def pn_link_name(link: pn_link_t) -> str: ...
# pn_link_t *pn_link_next(pn_link_t *link, pn_state_t state);
def pn_link_next(link: pn_link_t, state: pn_state_t) -> pn_link_t: ...
# void pn_link_offered(pn_link_t *sender, int credit);
def pn_link_offered(sender: pn_link_t, credit: int) -> None: ...
# void pn_link_open(pn_link_t *link);
def pn_link_open(link: pn_link_t) -> None: ...
# pn_data_t *pn_link_properties(pn_link_t *link);
def pn_link_properties(link: pn_link_t) -> pn_data_t: ...
# int pn_link_queued(pn_link_t *link);
def pn_link_queued(link: pn_link_t) -> int: ...
# pn_rcv_settle_mode_t pn_link_rcv_settle_mode(pn_link_t *link);
def pn_link_rcv_settle_mode(link: pn_link_t) -> pn_rcv_settle_mode_t: ...
# ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n);
def pn_link_recv(receiver: pn_link_t, bytes: str, n: int) -> int: ...
# pn_condition_t *pn_link_remote_condition(pn_link_t *link);
def pn_link_remote_condition(link: pn_link_t) -> pn_condition_t: ...
# uint64_t pn_link_remote_max_message_size(pn_link_t *link);
def pn_link_remote_max_message_size(link: pn_link_t) -> int: ...
# pn_data_t *pn_link_remote_properties(pn_link_t *link);
def pn_link_remote_properties(link: pn_link_t) -> pn_data_t: ...
# pn_rcv_settle_mode_t pn_link_remote_rcv_settle_mode(pn_link_t *link);
def pn_link_remote_rcv_settle_mode(link: pn_link_t) -> pn_rcv_settle_mode_t: ...
# pn_snd_settle_mode_t pn_link_remote_snd_settle_mode(pn_link_t *link);
def pn_link_remote_snd_settle_mode(link: pn_link_t) -> pn_snd_settle_mode_t: ...
# pn_terminus_t *pn_link_remote_source(pn_link_t *link);
def pn_link_remote_source(link: pn_link_t) -> pn_terminus_t: ...
# pn_terminus_t *pn_link_remote_target(pn_link_t *link);
def pn_link_remote_target(link: pn_link_t) -> pn_terminus_t: ...
# ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n);
def pn_link_send(sender: pn_link_t, bytes: str, n: int) -> int: ...
# pn_session_t *pn_link_session(pn_link_t *link);
def pn_link_session(link: pn_link_t) -> pn_session_t: ...
# void pn_link_set_drain(pn_link_t *receiver, _Bool drain);
def pn_link_set_drain(receiver: pn_link_t, drain: bool) -> None: ...
# void pn_link_set_max_message_size(pn_link_t *link, uint64_t size);
def pn_link_set_max_message_size(link: pn_link_t, size: int) -> None: ...
# void pn_link_set_rcv_settle_mode(pn_link_t *link, pn_rcv_settle_mode_t mode);
def pn_link_set_rcv_settle_mode(link: pn_link_t, mode: pn_rcv_settle_mode_t) -> None: ...
# void pn_link_set_snd_settle_mode(pn_link_t *link, pn_snd_settle_mode_t mode);
def pn_link_set_snd_settle_mode(link: pn_link_t, mode: pn_snd_settle_mode_t) -> None: ...
# pn_snd_settle_mode_t pn_link_snd_settle_mode(pn_link_t *link);
def pn_link_snd_settle_mode(link: pn_link_t) -> pn_snd_settle_mode_t: ...
# pn_terminus_t *pn_link_source(pn_link_t *link);
def pn_link_source(link: pn_link_t) -> pn_terminus_t: ...
# pn_state_t pn_link_state(pn_link_t *link);
def pn_link_state(link: pn_link_t) -> pn_state_t: ...
# pn_terminus_t *pn_link_target(pn_link_t *link);
def pn_link_target(link: pn_link_t) -> pn_terminus_t: ...
# int pn_link_unsettled(pn_link_t *link);
def pn_link_unsettled(link: pn_link_t) -> int: ...


# pn_message_t *pn_message(void);
def pn_message() -> pn_message_t: ...
# pn_data_t *pn_message_annotations(pn_message_t *msg);
def pn_message_annotations(msg: pn_message_t) -> pn_data_t: ...
# pn_data_t *pn_message_body(pn_message_t *msg);
def pn_message_body(msg: pn_message_t) -> pn_data_t: ...
# void pn_message_clear(pn_message_t *msg);
def pn_message_clear(msg: pn_message_t) -> None: ...
# int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size);
def pn_message_decode(msg: pn_message_t, bytes: str, size: int) -> int: ...
# pn_error_t *pn_message_error(pn_message_t *msg);
def pn_message_error(msg: pn_message_t) -> pn_error_t: ...
# void pn_message_free(pn_message_t *msg);
def pn_message_free(msg: pn_message_t) -> None: ...
# const char *pn_message_get_address(pn_message_t *msg);
def pn_message_get_address(msg: pn_message_t) -> str: ...
# const char *pn_message_get_content_encoding(pn_message_t *msg);
def pn_message_get_content_encoding(msg: pn_message_t) -> str: ...
# const char *pn_message_get_content_type(pn_message_t *msg);
def pn_message_get_content_type(msg: pn_message_t) -> str: ...
# pn_msgid_t pn_message_get_correlation_id(pn_message_t *msg);
def pn_message_get_correlation_id(msg: pn_message_t) -> pn_msgid_t: ...
# pn_timestamp_t pn_message_get_creation_time(pn_message_t *msg);
def pn_message_get_creation_time(msg: pn_message_t) -> int: ...
# uint32_t pn_message_get_delivery_count(pn_message_t *msg);
def pn_message_get_delivery_count(msg: pn_message_t) -> int: ...
# pn_timestamp_t pn_message_get_expiry_time(pn_message_t *msg);
def pn_message_get_expiry_time(msg: pn_message_t) -> int: ...
# const char *pn_message_get_group_id(pn_message_t *msg);
def pn_message_get_group_id(msg: pn_message_t) -> str: ...
# pn_sequence_t pn_message_get_group_sequence(pn_message_t *msg);
def pn_message_get_group_sequence(msg: pn_message_t) -> int: ...
# pn_msgid_t pn_message_get_id(pn_message_t *msg);
def pn_message_get_id(msg: pn_message_t) -> pn_msgid_t: ...
# uint8_t pn_message_get_priority(pn_message_t *msg);
def pn_message_get_priority(msg: pn_message_t) -> int: ...
# const char *pn_message_get_reply_to(pn_message_t *msg);
def pn_message_get_reply_to(msg: pn_message_t) -> str: ...
# const char *pn_message_get_reply_to_group_id(pn_message_t *msg);
def pn_message_get_reply_to_group_id(msg: pn_message_t) -> str: ...
# const char *pn_message_get_subject(pn_message_t *msg);
def pn_message_get_subject(msg: pn_message_t) -> str: ...
# pn_millis_t pn_message_get_ttl(pn_message_t *msg);
def pn_message_get_ttl(msg: pn_message_t) -> int: ...
# pn_bytes_t pn_message_get_user_id(pn_message_t *msg);
def pn_message_get_user_id(msg: pn_message_t) -> pn_bytes_t: ...
# pn_data_t *pn_message_instructions(pn_message_t *msg);
def pn_message_instructions(msg: pn_message_t) -> pn_data_t: ...
# _Bool pn_message_is_durable(pn_message_t *msg);
def pn_message_is_durable(msg: pn_message_t) -> bool: ...
# _Bool pn_message_is_first_acquirer(pn_message_t *msg);
def pn_message_is_first_acquirer(msg: pn_message_t) -> bool: ...
# _Bool pn_message_is_inferred(pn_message_t *msg);
def pn_message_is_inferred(msg: pn_message_t) -> bool: ...
# pn_data_t *pn_message_properties(pn_message_t *msg);
def pn_message_properties(msg: pn_message_t) -> pn_data_t: ...
# int pn_message_set_address(pn_message_t *msg, const char *address);
def pn_message_set_address(msg: pn_message_t, address: str) -> int: ...
# int pn_message_set_content_encoding(pn_message_t *msg, const char *encoding);
def pn_message_set_content_encoding(msg: pn_message_t, encoding: str) -> int: ...
# int pn_message_set_content_type(pn_message_t *msg, const char *type);
def pn_message_set_content_type(msg: pn_message_t, type: str) -> int: ...
# int pn_message_set_correlation_id(pn_message_t *msg, pn_msgid_t id);
def pn_message_set_correlation_id(msg: pn_message_t, id: pn_msgid_t) -> int: ...
# int pn_message_set_creation_time(pn_message_t *msg, pn_timestamp_t time);
def pn_message_set_creation_time(msg: pn_message_t, time: int) -> int: ...
# int pn_message_set_delivery_count(pn_message_t *msg, uint32_t count);
def pn_message_set_delivery_count(msg: pn_message_t, count: int) -> int: ...
# int pn_message_set_durable(pn_message_t *msg, _Bool durable);
def pn_message_set_durable(msg: pn_message_t, durable: bool) -> int: ...
# int pn_message_set_expiry_time(pn_message_t *msg, pn_timestamp_t time);
def pn_message_set_expiry_time(msg: pn_message_t, time: int) -> int: ...
# int pn_message_set_first_acquirer(pn_message_t *msg, _Bool first);
def pn_message_set_first_acquirer(msg: pn_message_t, first: bool) -> int: ...
# int pn_message_set_group_id(pn_message_t *msg, const char *group_id);
def pn_message_set_group_id(msg: pn_message_t, group_id: str) -> int: ...
# int pn_message_set_group_sequence(pn_message_t *msg, pn_sequence_t n);
def pn_message_set_group_sequence(msg: pn_message_t, n: int) -> int: ...
# int pn_message_set_id(pn_message_t *msg, pn_msgid_t id);
def pn_message_set_id(msg: pn_message_t, id: pn_msgid_t) -> int: ...
# int pn_message_set_inferred(pn_message_t *msg, _Bool inferred);
def pn_message_set_inferred(msg: pn_message_t, inferred: bool) -> int: ...
# int pn_message_set_priority(pn_message_t *msg, uint8_t priority);
def pn_message_set_priority(msg: pn_message_t, priority: int) -> int: ...
# int pn_message_set_reply_to(pn_message_t *msg, const char *reply_to);
def pn_message_set_reply_to(msg: pn_message_t, reply_to: str) -> int: ...
# int pn_message_set_reply_to_group_id(pn_message_t *msg, const char *reply_to_group_id);
def pn_message_set_reply_to_group_id(msg: pn_message_t, reply_to_group_id: str) -> int: ...
# int pn_message_set_subject(pn_message_t *msg, const char *subject);
def pn_message_set_subject(msg: pn_message_t, subject: str) -> int: ...
# int pn_message_set_ttl(pn_message_t *msg, pn_millis_t ttl);
def pn_message_set_ttl(msg: pn_message_t, ttl: int) -> int: ...
# int pn_message_set_user_id(pn_message_t *msg, pn_bytes_t user_id);
def pn_message_set_user_id(msg: pn_message_t, user_id: pn_bytes_t) -> int: ...


# pn_link_t *pn_receiver(pn_session_t *session, const char *name);
def pn_receiver(session: pn_session_t, name: str) -> pn_link_t: ...


# pn_sasl_t *pn_sasl(pn_transport_t *transport);
def pn_sasl(transport: pn_transport_t) -> pn_sasl_t: ...
# void pn_sasl_allowed_mechs(pn_sasl_t *sasl, const char *mechs);
def pn_sasl_allowed_mechs(sasl: pn_sasl_t, mechs: str) -> None: ...
# void pn_sasl_config_name(pn_sasl_t *sasl, const char *name);
def pn_sasl_config_name(sasl: pn_sasl_t, name: str) -> None: ...
# void pn_sasl_config_path(pn_sasl_t *sasl, const char *path);
def pn_sasl_config_path(sasl: pn_sasl_t, path: str) -> None: ...
# void pn_sasl_done(pn_sasl_t *sasl, pn_sasl_outcome_t outcome);
def pn_sasl_done(sasl: pn_sasl_t, outcome: pn_sasl_outcome_t) -> None: ...
# _Bool pn_sasl_extended(void);
def pn_sasl_extended() -> bool: ...
# _Bool pn_sasl_get_allow_insecure_mechs(pn_sasl_t *sasl);
def pn_sasl_get_allow_insecure_mechs(sasl: pn_sasl_t) -> bool: ...
# const char *pn_sasl_get_authorization(pn_sasl_t *sasl);
def pn_sasl_get_authorization(sasl: pn_sasl_t) -> str: ...
# const char *pn_sasl_get_mech(pn_sasl_t *sasl);
def pn_sasl_get_mech(sasl: pn_sasl_t) -> str: ...
# const char *pn_sasl_get_user(pn_sasl_t *sasl);
def pn_sasl_get_user(sasl: pn_sasl_t) -> str: ...
# pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl);
def pn_sasl_outcome(sasl: pn_sasl_t) -> pn_sasl_outcome_t: ...
# void pn_sasl_set_allow_insecure_mechs(pn_sasl_t *sasl, _Bool insecure);
def pn_sasl_set_allow_insecure_mechs(sasl: pn_sasl_t, insecure: bool) -> None: ...


# pn_link_t *pn_sender(pn_session_t *session, const char *name);
def pn_sender(session: pn_session_t, name: str) -> pn_link_t: ...


# pn_session_t *pn_session(pn_connection_t *connection);
def pn_session(connection: pn_connection_t) -> pn_session_t: ...
# pn_record_t *pn_session_attachments(pn_session_t *session);
def pn_session_attachments(session: pn_session_t) -> pn_record_t: ...
# void pn_session_close(pn_session_t *session);
def pn_session_close(session: pn_session_t) -> None: ...
# pn_condition_t *pn_session_condition(pn_session_t *session);
def pn_session_condition(session: pn_session_t) -> pn_condition_t: ...
# pn_connection_t *pn_session_connection(pn_session_t *session);
def pn_session_connection(session: pn_session_t) -> pn_connection_t: ...
# void pn_session_free(pn_session_t *session);
def pn_session_free(session: pn_session_t) -> None: ...
# size_t pn_session_get_incoming_capacity(pn_session_t *session);
def pn_session_get_incoming_capacity(session: pn_session_t) -> int: ...
# size_t pn_session_get_outgoing_window(pn_session_t *session);
def pn_session_get_outgoing_window(session: pn_session_t) -> int: ...
# pn_session_t *pn_session_head(pn_connection_t *connection, pn_state_t state);
def pn_session_head(connection: pn_connection_t, state: pn_state_t) -> pn_session_t: ...
# size_t pn_session_incoming_bytes(pn_session_t *session);
def pn_session_incoming_bytes(session: pn_session_t) -> int: ...
# pn_session_t *pn_session_next(pn_session_t *session, pn_state_t state);
def pn_session_next(session: pn_session_t, state: pn_state_t) -> pn_session_t: ...
# void pn_session_open(pn_session_t *session);
def pn_session_open(session: pn_session_t) -> None: ...
# size_t pn_session_outgoing_bytes(pn_session_t *session);
def pn_session_outgoing_bytes(session: pn_session_t) -> int: ...
# pn_condition_t *pn_session_remote_condition(pn_session_t *session);
def pn_session_remote_condition(session: pn_session_t) -> pn_condition_t: ...
# void pn_session_set_incoming_capacity(pn_session_t *session, size_t capacity);
def pn_session_set_incoming_capacity(session: pn_session_t, capacity: int) -> None: ...
# void pn_session_set_outgoing_window(pn_session_t *session, size_t window);
def pn_session_set_outgoing_window(session: pn_session_t, window: int) -> None: ...
# pn_state_t pn_session_state(pn_session_t *session);
def pn_session_state(session: pn_session_t) -> pn_state_t: ...


# pn_ssl_t *pn_ssl(pn_transport_t *transport);
def pn_ssl(transport: pn_transport_t) -> pn_ssl_t: ...
# pn_ssl_domain_t *pn_ssl_domain(pn_ssl_mode_t mode);
def pn_ssl_domain(mode: pn_ssl_mode_t) -> pn_ssl_domain_t: ...
# int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain);
def pn_ssl_domain_allow_unsecured_client(domain: pn_ssl_domain_t) -> int: ...
# void pn_ssl_domain_free(pn_ssl_domain_t *domain);
def pn_ssl_domain_free(domain: pn_ssl_domain_t) -> None: ...
# int pn_ssl_domain_set_ciphers(pn_ssl_domain_t *domain, const char *ciphers);
def pn_ssl_domain_set_ciphers(domain: pn_ssl_domain_t, ciphers: str) -> int: ...
# int pn_ssl_domain_set_credentials(pn_ssl_domain_t *domain, const char *credential_1, const char *credential_2,
#                                   const char *password);
def pn_ssl_domain_set_credentials(domain: pn_ssl_domain_t, credential_1: str, credential_2: str, password: str) -> int: ...
# int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain, const pn_ssl_verify_mode_t mode,
#                                           const char *trusted_CAs);
def pn_ssl_domain_set_peer_authentication(domain: pn_ssl_domain_t, mode: pn_ssl_verify_mode_t, trusted_CAs: str) -> int: ...
# int pn_ssl_domain_set_protocols(pn_ssl_domain_t *domain, const char *protocols);
def pn_ssl_domain_set_protocols(domain: pn_ssl_domain_t, protocols: str) -> int: ...
# int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain, const char *certificate_db);
def pn_ssl_domain_set_trusted_ca_db(domain: pn_ssl_domain_t, certificate_db: str) -> int: ...
# int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0, char *fingerprint, size_t fingerprint_length, pn_ssl_hash_alg hash_alg);
def pn_ssl_get_cert_fingerprint(ssl0: pn_ssl_t, fingerprint: str, fingerprint_len: int, hash: pn_ssl_hash_alg) -> int: ...
# _Bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size);
def pn_ssl_get_cipher_name(ssl: pn_ssl_t, buffer: str, size: int) -> bool: ...
# _Bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size);
def pn_ssl_get_protocol_name(ssl: pn_ssl_t, buffer: str, size: int) -> bool: ...
# const char *pn_ssl_get_remote_subject(pn_ssl_t *ssl);
def pn_ssl_get_remote_subject(ssl: pn_ssl_t) -> str: ...
# const char *pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field);
def pn_ssl_get_remote_subject_subfield(ssl0: pn_ssl_t, field: pn_ssl_cert_subject_subfield) -> str: ...
# int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_domain_t *domain, const char *session_id);
def pn_ssl_init(ssl: pn_ssl_t, domain: pn_ssl_domain_t, session_id: str) -> int: ...
# _Bool pn_ssl_present(void);
def pn_ssl_present() -> bool: ...
# pn_ssl_resume_status_t pn_ssl_resume_status(pn_ssl_t *ssl);
def pn_ssl_resume_status(ssl: pn_ssl_t) -> pn_ssl_resume_status_t: ...
# int pn_ssl_set_peer_hostname(pn_ssl_t *ssl, const char *hostname);
def pn_ssl_set_peer_hostname(ssl: pn_ssl_t, hostname: str) -> int: ...


# pn_data_t *pn_terminus_capabilities(pn_terminus_t *terminus);
def pn_terminus_capabilities(terminus: pn_terminus_t) -> pn_data_t: ...
# int pn_terminus_copy(pn_terminus_t *terminus, pn_terminus_t *src);
def pn_terminus_copy(terminus: pn_terminus_t, src: pn_terminus_t) -> int: ...
# pn_data_t *pn_terminus_filter(pn_terminus_t *terminus);
def pn_terminus_filter(terminus: pn_terminus_t) -> pn_data_t: ...
# const char *pn_terminus_get_address(pn_terminus_t *terminus);
def pn_terminus_get_address(terminus: pn_terminus_t) -> str: ...
# pn_distribution_mode_t pn_terminus_get_distribution_mode(const pn_terminus_t *terminus);
def pn_terminus_get_distribution_mode(terminus: pn_terminus_t) -> pn_distribution_mode_t: ...
# pn_durability_t pn_terminus_get_durability(pn_terminus_t *terminus);
def pn_terminus_get_durability(terminus: pn_terminus_t) -> pn_durability_t: ...
# pn_expiry_policy_t pn_terminus_get_expiry_policy(pn_terminus_t *terminus);
def pn_terminus_get_expiry_policy(terminus: pn_terminus_t) -> pn_expiry_policy_t: ...
# pn_seconds_t pn_terminus_get_timeout(pn_terminus_t *terminus);
def pn_terminus_get_timeout(terminus: pn_terminus_t) -> int: ...
# pn_terminus_type_t pn_terminus_get_type(pn_terminus_t *terminus);
def pn_terminus_get_type(terminus: pn_terminus_t) -> pn_terminus_type_t: ...
# _Bool pn_terminus_is_dynamic(pn_terminus_t *terminus);
def pn_terminus_is_dynamic(terminus: pn_terminus_t) -> bool: ...
# pn_data_t *pn_terminus_outcomes(pn_terminus_t *terminus);
def pn_terminus_outcomes(terminus: pn_terminus_t) -> pn_data_t: ...
# pn_data_t *pn_terminus_properties(pn_terminus_t *terminus);
def pn_terminus_properties(terminus: pn_terminus_t) -> pn_data_t: ...
# int pn_terminus_set_address(pn_terminus_t *terminus, const char *address);
def pn_terminus_set_address(terminus: pn_terminus_t, address: str) -> int: ...
# int pn_terminus_set_distribution_mode(pn_terminus_t *terminus, pn_distribution_mode_t mode);
def pn_terminus_set_distribution_mode(terminus: pn_terminus_t, mode: pn_distribution_mode_t) -> int: ...
# int pn_terminus_set_durability(pn_terminus_t *terminus, pn_durability_t durability);
def pn_terminus_set_durability(terminus: pn_terminus_t, durability: pn_durability_t) -> int: ...
# int pn_terminus_set_dynamic(pn_terminus_t *terminus, _Bool dynamic);
def pn_terminus_set_dynamic(terminus: pn_terminus_t, dynamic: bool) -> int: ...
# int pn_terminus_set_expiry_policy(pn_terminus_t *terminus, pn_expiry_policy_t policy);
def pn_terminus_set_expiry_policy(terminus: pn_terminus_t, policy: pn_expiry_policy_t) -> int: ...
# int pn_terminus_set_timeout(pn_terminus_t *terminus, pn_seconds_t timeout);
def pn_terminus_set_timeout(terminus: pn_terminus_t, timeout: int) -> int: ...
# int pn_terminus_set_type(pn_terminus_t *terminus, pn_terminus_type_t type);
def pn_terminus_set_type(terminus: pn_terminus_t, type: pn_terminus_type_t) -> int: ...


# pn_transport_t *pn_transport(void);
def pn_transport() -> pn_transport_t: ...
# pn_record_t *pn_transport_attachments(pn_transport_t *transport);
def pn_transport_attachments(transport: pn_transport_t) -> pn_record_t: ...
# int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection);
def pn_transport_bind(transport: pn_transport_t, connection: pn_connection_t) -> int: ...
# ssize_t pn_transport_capacity(pn_transport_t *transport);
def pn_transport_capacity(transport: pn_transport_t) -> int: ...
# int pn_transport_close_head(pn_transport_t *transport);
def pn_transport_close_head(transport: pn_transport_t) -> int: ...
# int pn_transport_close_tail(pn_transport_t *transport);
def pn_transport_close_tail(transport: pn_transport_t) -> int: ...
# _Bool pn_transport_closed(pn_transport_t *transport);
def pn_transport_closed(transport: pn_transport_t) -> bool: ...
# pn_condition_t *pn_transport_condition(pn_transport_t *transport);
def pn_transport_condition(transport: pn_transport_t) -> pn_condition_t: ...
# pn_connection_t *pn_transport_connection(pn_transport_t *transport);
def pn_transport_connection(transport: pn_transport_t) -> pn_connection_t: ...
# pn_error_t *pn_transport_error(pn_transport_t *transport);
def pn_transport_error(transport: pn_transport_t) -> pn_error_t: ...
# uint16_t pn_transport_get_channel_max(pn_transport_t *transport);
def pn_transport_get_channel_max(transport: pn_transport_t) -> int: ...
# uint64_t pn_transport_get_frames_input(const pn_transport_t *transport);
def pn_transport_get_frames_input(transport: pn_transport_t) -> int: ...
# uint64_t pn_transport_get_frames_output(const pn_transport_t *transport);
def pn_transport_get_frames_output(transport: pn_transport_t) -> int: ...
# pn_millis_t pn_transport_get_idle_timeout(pn_transport_t *transport);
def pn_transport_get_idle_timeout(transport: pn_transport_t) -> int: ...
# uint32_t pn_transport_get_max_frame(pn_transport_t *transport);
def pn_transport_get_max_frame(transport: pn_transport_t) -> int: ...
# pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport);
def pn_transport_get_remote_idle_timeout(transport: pn_transport_t) -> int: ...
# uint32_t pn_transport_get_remote_max_frame(pn_transport_t *transport);
def pn_transport_get_remote_max_frame(transport: pn_transport_t) -> int: ...
# const char *pn_transport_get_user(pn_transport_t *transport);
def pn_transport_get_user(transport: pn_transport_t) -> str: ...
# _Bool pn_transport_is_authenticated(pn_transport_t *transport);
def pn_transport_is_authenticated(transport: pn_transport_t) -> bool: ...
# _Bool pn_transport_is_encrypted(pn_transport_t *transport);
def pn_transport_is_encrypted(transport: pn_transport_t) -> bool: ...
# void pn_transport_log(pn_transport_t *transport, const char *message);
def pn_transport_log(transport: pn_transport_t, message: str) -> None: ...
# ssize_t pn_transport_peek(pn_transport_t *transport, char *dst, size_t size);
def pn_transport_peek(transport: pn_transport_t, dst: str, size: int) -> int: ...
# ssize_t pn_transport_pending(pn_transport_t *transport);
def pn_transport_pending(transport: pn_transport_t) -> int: ...
# void pn_transport_pop(pn_transport_t *transport, size_t size);
def pn_transport_pop(transport: pn_transport_t, size: int) -> None: ...
# ssize_t pn_transport_push(pn_transport_t *transport, const char *src, size_t size);
def pn_transport_push(transport: pn_transport_t, src: str, size: int) -> int: ...
# uint16_t pn_transport_remote_channel_max(pn_transport_t *transport);
def pn_transport_remote_channel_max(transport: pn_transport_t) -> int: ...
# void pn_transport_require_auth(pn_transport_t *transport, _Bool required);
def pn_transport_require_auth(transport: pn_transport_t, required: bool) -> None: ...
# void pn_transport_require_encryption(pn_transport_t *transport, _Bool required);
def pn_transport_require_encryption(transport: pn_transport_t, required: bool) -> None: ...
# int pn_transport_set_channel_max(pn_transport_t *transport, uint16_t channel_max);
def pn_transport_set_channel_max(transport: pn_transport_t, channel_max: int) -> int: ...
# void pn_transport_set_idle_timeout(pn_transport_t *transport, pn_millis_t timeout);
def pn_transport_set_idle_timeout(transport: pn_transport_t, timeout: int) -> None: ...
# void pn_transport_set_max_frame(pn_transport_t *transport, uint32_t size);
def pn_transport_set_max_frame(transport: pn_transport_t, size: int) -> None: ...
# void pn_transport_set_server(pn_transport_t *transport);
def pn_transport_set_server(transport: pn_transport_t) -> None: ...
# void pn_transport_set_tracer(pn_transport_t *transport, pn_tracer_t tracer);
# def pn_transport_set_tracer(transport: pn_transport_t, tracer: pn_tracer_t) -> None: ...
# int64_t pn_transport_tick(pn_transport_t *transport, int64_t now);
def pn_transport_tick(transport: pn_transport_t, now: int) -> int: ...
# void pn_transport_trace(pn_transport_t *transport, pn_trace_t trace);
def pn_transport_trace(transport: pn_transport_t, trace: pn_trace_t) -> None: ...
# int pn_transport_unbind(pn_transport_t *transport);
def pn_transport_unbind(transport: pn_transport_t) -> int: ...


# Dispositions defined in C macros
# results of pn_disposition_type
PN_RECEIVED: int
PN_ACCEPTED: int
PN_REJECTED: int
PN_RELEASED: int
PN_MODIFIED: int

# Default message priority
PN_DEFAULT_PRIORITY: int

# Returned errors
PN_OK: int
PN_EOS: int
PN_OVERFLOW: int
PN_TIMEOUT: int
PN_INTR: int
PN_LOCAL_UNINIT: int
PN_LOCAL_ACTIVE: int
PN_LOCAL_CLOSED: int
PN_REMOTE_UNINIT: int
PN_REMOTE_ACTIVE: int
PN_REMOTE_CLOSED: int
PN_TRACE_OFF: int
PN_TRACE_RAW: int
PN_TRACE_FRM: int
PN_TRACE_DRV: int

# Maybe need to get this from cmake, or modify how the binding does this
PN_VERSION_MAJOR: int
PN_VERSION_MINOR: int
PN_VERSION_POINT: int

# // Initialization of library - probably a better way to do this than explicitly, but it works!
# void init();

# pn_connection_t *pn_cast_pn_connection(void *x);
# pn_session_t *pn_cast_pn_session(void *x);
# pn_link_t *pn_cast_pn_link(void *x);
# pn_delivery_t *pn_cast_pn_delivery(void *x);
# pn_transport_t *pn_cast_pn_transport(void *x);

# extern "Python" void pn_pyref_incref(void *object);
# extern "Python" void pn_pyref_decref(void *object);
# extern "Python" void pn_pytracer(pn_transport_t *transport, const char *message);

# pn_event_t *pn_collector_put_py(pn_collector_t *collector, void *context, pn_event_type_t type);
# ssize_t pn_data_format_py(pn_data_t *data, char *bytes, size_t size);
# const char *pn_event_class_name_py(pn_event_t *event);
# ssize_t pn_message_encode_py(pn_message_t *msg, char *bytes, size_t size);
# void pn_record_def_py(pn_record_t *record);
# void *pn_record_get_py(pn_record_t *record);
# void pn_record_set_py(pn_record_t *record, void *value);
# int pn_ssl_get_peer_hostname_py(pn_ssl_t *ssl, char *hostname, size_t size);


def isnull(obj: Any) -> bool: ...
def addressof(obj): ...
def void2py(void): ...
def string2utf8(string): ...
def utf82string(string): ...
def bytes2py(b): ...
def bytes2pybytes(b): ...
def bytes2string(b, encoding: str = 'utf8'): ...
def py2bytes(py): ...
def string2bytes(py, encoding: str = 'utf8'): ...
def UUID2uuid(py): ...
def uuid2bytes(uuid): ...
def decimal1282py(decimal128): ...
def py2decimal128(py): ...
def msgid2py(msgid): ...
def py2msgid(py): ...
def pn_pytracer(transport, message) -> None: ...
def pn_transport_get_pytracer(transport): ...
def pn_transport_set_pytracer(transport, tracer) -> None: ...


retained_objects: set[Any]


def clear_retained_objects() -> None: ...
def retained_count() -> int: ...
def pn_pyref_incref(obj) -> None: ...
def pn_pyref_decref(obj) -> None: ...
# def pn_tostring(obj): ...
def pn_collector_put_pyref(collector, obj, etype) -> None: ...
def pn_record_get_py(record): ...
def pn_event_class_name(event): ...
# def pn_transport_peek(transport, size): ...
# def pn_transport_push(transport, src): ...
# def pn_message_decode(msg, buff): ...
def pn_message_encode(msg, size): ...
# def pn_data_decode(data, buff): ...
# def pn_data_encode(data, size): ...
def pn_data_format(data, size): ...
# def pn_link_recv(receiver, limit): ...
# def pn_link_send(sender, buff): ...
# def pn_condition_set_name(cond, name): ...
# def pn_condition_set_description(cond, description): ...
# def pn_condition_get_name(cond): ...
# def pn_condition_get_description(cond): ...
# def pn_error_text(error): ...
# def pn_data_lookup(data, name): ...
# def pn_data_put_decimal128(data, d): ...
# def pn_data_put_uuid(data, u): ...
# def pn_data_put_binary(data, b): ...
# def pn_data_put_string(data, s): ...
# def pn_data_put_symbol(data, s): ...
# def pn_data_get_decimal128(data): ...
# def pn_data_get_uuid(data): ...
# def pn_data_get_binary(data): ...
# def pn_data_get_string(data): ...
# def pn_data_get_symbol(data): ...
# def pn_delivery_tag(delivery): ...
# def pn_connection_get_container(connection): ...
# def pn_connection_set_container(connection, name) -> None: ...
# def pn_connection_get_hostname(connection): ...
# def pn_connection_set_hostname(connection, name) -> None: ...
# def pn_connection_get_user(connection): ...
# def pn_connection_set_user(connection, name) -> None: ...
# def pn_connection_get_authorization(connection): ...
# def pn_connection_set_authorization(connection, name) -> None: ...
# def pn_connection_set_password(connection, name) -> None: ...
# def pn_connection_remote_container(connection): ...
# def pn_connection_remote_hostname(connection): ...
# def pn_sender(session, name): ...
# def pn_receiver(session, name): ...
# def pn_delivery(link, tag): ...
# def pn_link_name(link): ...
# def pn_terminus_get_address(terminus): ...
# def pn_terminus_set_address(terminus, address): ...
# def pn_event_type_name(number): ...
# def pn_message_get_id(message): ...
# def pn_message_set_id(message, value) -> None: ...
# def pn_message_get_user_id(message): ...
# def pn_message_set_user_id(message, value): ...
# def pn_message_get_address(message): ...
# def pn_message_set_address(message, value): ...
# def pn_message_get_subject(message): ...
# def pn_message_set_subject(message, value): ...
# def pn_message_get_reply_to(message): ...
# def pn_message_set_reply_to(message, value): ...
# def pn_message_get_correlation_id(message): ...
# def pn_message_set_correlation_id(message, value) -> None: ...
# def pn_message_get_content_type(message): ...
# def pn_message_set_content_type(message, value): ...
# def pn_message_get_content_encoding(message): ...
# def pn_message_set_content_encoding(message, value): ...
# def pn_message_get_group_id(message): ...
# def pn_message_set_group_id(message, value): ...
# def pn_message_get_reply_to_group_id(message): ...
# def pn_message_set_reply_to_group_id(message, value): ...
# def pn_transport_log(transport, message) -> None: ...
# def pn_transport_get_user(transport): ...
# def pn_sasl_get_user(sasl): ...
# def pn_sasl_get_authorization(sasl): ...
# def pn_sasl_get_mech(sasl): ...
# def pn_sasl_allowed_mechs(sasl, mechs) -> None: ...
# def pn_sasl_config_name(sasl, name) -> None: ...
# def pn_sasl_config_path(sasl, path) -> None: ...
# def pn_ssl_domain_set_credentials(domain, cert_file, key_file, password): ...
# def pn_ssl_domain_set_trusted_ca_db(domain, certificate_db): ...
# def pn_ssl_domain_set_peer_authentication(domain, verify_mode, trusted_CAs): ...
# def pn_ssl_init(ssl, domain, session_id) -> None: ...
# def pn_ssl_get_remote_subject_subfield(ssl, subfield_name): ...
# def pn_ssl_get_remote_subject(ssl): ...
# def pn_ssl_domain_set_protocols(domain, protocols): ...
# def pn_ssl_domain_set_ciphers(domain, ciphers): ...
# def pn_ssl_get_cipher_name(ssl, size): ...
# def pn_ssl_get_protocol_name(ssl, size): ...
# def pn_ssl_get_cert_fingerprint(ssl, fingerprint_len, hash_alg): ...
def pn_ssl_get_peer_hostname(ssl, size): ...
# def pn_ssl_set_peer_hostname(ssl, hostname): ...
