from cproton_ffi import lib
from cproton_ffi import ffi
from cproton_ffi.lib import \
    PN_VERSION_MAJOR, PN_VERSION_MINOR, PN_VERSION_POINT, \
    PN_ACCEPTED, PN_MODIFIED, PN_RECEIVED, PN_REJECTED, PN_RELEASED, pn_delivery_abort, \
    pn_delivery_aborted, pn_delivery_attachments, pn_delivery_link, pn_delivery_local, pn_delivery_local_state, \
    pn_delivery_partial, pn_delivery_pending, pn_delivery_readable, pn_delivery_remote, pn_delivery_remote_state, \
    pn_delivery_settle, pn_delivery_settled, pn_delivery_tag, pn_delivery_update, pn_delivery_updated, \
    pn_delivery_writable, pn_disposition_annotations, pn_disposition_condition, pn_disposition_data, \
    pn_disposition_get_section_number, pn_disposition_get_section_offset, pn_disposition_is_failed, \
    pn_disposition_is_undeliverable, pn_disposition_set_failed, pn_disposition_set_section_number, \
    pn_disposition_set_section_offset, pn_disposition_set_undeliverable, pn_disposition_type, pn_work_next, \
    PN_CONFIGURATION, PN_COORDINATOR, PN_DELIVERIES, PN_DIST_MODE_COPY, PN_DIST_MODE_MOVE, \
    PN_DIST_MODE_UNSPECIFIED, PN_EOS, PN_EXPIRE_NEVER, PN_EXPIRE_WITH_CONNECTION, PN_EXPIRE_WITH_LINK, \
    PN_EXPIRE_WITH_SESSION, PN_LOCAL_ACTIVE, PN_LOCAL_CLOSED, PN_LOCAL_UNINIT, PN_NONDURABLE, PN_RCV_FIRST, \
    PN_RCV_SECOND, PN_REMOTE_ACTIVE, PN_REMOTE_CLOSED, PN_REMOTE_UNINIT, PN_SND_MIXED, PN_SND_SETTLED, PN_SND_UNSETTLED, \
    PN_SOURCE, PN_TARGET, PN_UNSPECIFIED, pn_connection, pn_connection_attachments, pn_connection_close, \
    pn_connection_collect, pn_connection_condition, pn_connection_desired_capabilities, pn_connection_error, \
    pn_connection_get_authorization, pn_connection_get_container, pn_connection_get_hostname, pn_connection_get_user, \
    pn_connection_offered_capabilities, \
    pn_connection_open, pn_connection_properties, pn_connection_release, pn_connection_remote_condition, \
    pn_connection_remote_container, pn_connection_remote_desired_capabilities, pn_connection_remote_hostname, \
    pn_connection_remote_offered_capabilities, pn_connection_remote_properties, \
    pn_connection_set_authorization, pn_connection_set_container, \
    pn_connection_set_hostname, pn_connection_set_password, pn_connection_set_user, pn_connection_state, \
    pn_connection_transport, pn_delivery, pn_error_code, pn_error_text, pn_link_advance, pn_link_attachments, \
    pn_link_available, pn_link_close, pn_link_condition, pn_link_credit, pn_link_current, pn_link_detach, pn_link_drain, \
    pn_link_drained, pn_link_draining, pn_link_error, pn_link_flow, pn_link_free, pn_link_get_drain, pn_link_head, \
    pn_link_is_receiver, pn_link_is_sender, pn_link_max_message_size, pn_link_name, pn_link_next, pn_link_offered, \
    pn_link_open, pn_link_queued, pn_link_rcv_settle_mode, pn_link_recv, pn_link_remote_condition, \
    pn_link_remote_max_message_size, pn_link_remote_rcv_settle_mode, pn_link_remote_snd_settle_mode, \
    pn_link_remote_source, pn_link_remote_target, pn_link_send, pn_link_session, pn_link_set_drain, \
    pn_link_set_max_message_size, pn_link_set_rcv_settle_mode, pn_link_set_snd_settle_mode, pn_link_snd_settle_mode, \
    pn_link_source, pn_link_state, pn_link_target, pn_link_unsettled, pn_receiver, pn_sender, pn_session, \
    pn_session_attachments, pn_session_close, pn_session_condition, pn_session_connection, pn_session_free, \
    pn_session_get_incoming_capacity, pn_session_get_outgoing_window, pn_session_head, pn_session_incoming_bytes, \
    pn_session_next, pn_session_open, pn_session_outgoing_bytes, pn_session_remote_condition, \
    pn_session_set_incoming_capacity, pn_session_set_outgoing_window, pn_session_state, pn_terminus_capabilities, \
    pn_terminus_copy, pn_terminus_filter, pn_terminus_get_address, pn_terminus_get_distribution_mode, \
    pn_terminus_get_durability, pn_terminus_get_expiry_policy, pn_terminus_get_timeout, pn_terminus_get_type, \
    pn_terminus_is_dynamic, pn_terminus_outcomes, pn_terminus_properties, pn_terminus_set_address, \
    pn_terminus_set_distribution_mode, pn_terminus_set_durability, pn_terminus_set_dynamic, \
    pn_terminus_set_expiry_policy, pn_terminus_set_timeout, pn_terminus_set_type, pn_work_head, \
    pn_link_properties, pn_link_remote_properties, \
    pn_condition_clear, pn_condition_set_name, pn_condition_set_description, pn_condition_info, \
    pn_condition_is_set, pn_condition_get_name, pn_condition_get_description, \
    PN_ARRAY, PN_BINARY, PN_BOOL, PN_BYTE, PN_CHAR, PN_DECIMAL128, PN_DECIMAL32, PN_DECIMAL64, \
    PN_DESCRIBED, PN_DOUBLE, PN_FLOAT, PN_INT, PN_LIST, PN_LONG, PN_MAP, PN_NULL, PN_OVERFLOW, PN_SHORT, PN_STRING, \
    PN_SYMBOL, PN_TIMESTAMP, PN_UBYTE, PN_UINT, PN_ULONG, PN_USHORT, PN_UUID, pn_data, pn_data_clear, pn_data_copy, \
    pn_data_decode, pn_data_dump, pn_data_encode, pn_data_encoded_size, pn_data_enter, pn_data_error, pn_data_exit, \
    pn_data_format, pn_data_free, pn_data_get_array, pn_data_get_array_type, pn_data_get_binary, pn_data_get_bool, \
    pn_data_get_byte, pn_data_get_char, pn_data_get_decimal128, pn_data_get_decimal32, pn_data_get_decimal64, \
    pn_data_get_double, pn_data_get_float, pn_data_get_int, pn_data_get_list, pn_data_get_long, pn_data_get_map, \
    pn_data_get_short, pn_data_get_string, pn_data_get_symbol, pn_data_get_timestamp, pn_data_get_ubyte, \
    pn_data_get_uint, pn_data_get_ulong, pn_data_get_ushort, pn_data_get_uuid, pn_data_is_array_described, \
    pn_data_is_described, pn_data_is_null, pn_data_lookup, pn_data_narrow, pn_data_next, pn_data_prev, \
    pn_data_put_array, pn_data_put_binary, pn_data_put_bool, pn_data_put_byte, pn_data_put_char, pn_data_put_decimal128, \
    pn_data_put_decimal32, pn_data_put_decimal64, pn_data_put_described, pn_data_put_double, pn_data_put_float, \
    pn_data_put_int, pn_data_put_list, pn_data_put_long, pn_data_put_map, pn_data_put_null, pn_data_put_short, \
    pn_data_put_string, pn_data_put_symbol, pn_data_put_timestamp, pn_data_put_ubyte, pn_data_put_uint, \
    pn_data_put_ulong, pn_data_put_ushort, pn_data_put_uuid, pn_data_rewind, pn_data_type, pn_data_widen, pn_error_text, \
    PN_TIMEOUT, PN_INTR, \
    pn_incref, pn_decref, \
    pn_record_get, pn_record_def, pn_record_set, \
    PN_EOS, PN_OK, PN_SASL_AUTH, PN_SASL_NONE, PN_SASL_OK, PN_SASL_PERM, PN_SASL_SYS, PN_SASL_TEMP, \
    PN_SSL_ANONYMOUS_PEER, PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY, PN_SSL_CERT_SUBJECT_COMMON_NAME, \
    PN_SSL_CERT_SUBJECT_COUNTRY_NAME, PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME, PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT, \
    PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE, PN_SSL_MD5, PN_SSL_MODE_CLIENT, PN_SSL_MODE_SERVER, PN_SSL_RESUME_NEW, \
    PN_SSL_RESUME_REUSED, PN_SSL_RESUME_UNKNOWN, PN_SSL_SHA1, PN_SSL_SHA256, PN_SSL_SHA512, PN_SSL_VERIFY_PEER, \
    PN_SSL_VERIFY_PEER_NAME, PN_TRACE_DRV, PN_TRACE_FRM, PN_TRACE_OFF, PN_TRACE_RAW, pn_error_text, pn_sasl, \
    pn_sasl_allowed_mechs, pn_sasl_config_name, pn_sasl_config_path, pn_sasl_done, pn_sasl_extended, \
    pn_sasl_get_allow_insecure_mechs, pn_sasl_get_mech, pn_sasl_get_user, pn_sasl_get_authorization, pn_sasl_outcome, \
    pn_sasl_set_allow_insecure_mechs, pn_ssl, pn_ssl_domain, pn_ssl_domain_allow_unsecured_client, pn_ssl_domain_free, \
    pn_ssl_domain_set_credentials, pn_ssl_domain_set_peer_authentication, pn_ssl_domain_set_trusted_ca_db, \
    pn_ssl_get_cert_fingerprint, pn_ssl_get_cipher_name, pn_ssl_get_peer_hostname, pn_ssl_get_protocol_name, \
    pn_ssl_get_remote_subject, pn_ssl_get_remote_subject_subfield, pn_ssl_init, pn_ssl_present, pn_ssl_resume_status, \
    pn_ssl_set_peer_hostname, pn_transport, pn_transport_attachments, pn_transport_bind, pn_transport_capacity, \
    pn_transport_close_head, pn_transport_close_tail, pn_transport_closed, pn_transport_condition, \
    pn_transport_connection, pn_transport_error, pn_transport_get_channel_max, pn_transport_get_frames_input, \
    pn_transport_get_frames_output, pn_transport_get_idle_timeout, pn_transport_get_max_frame, \
    pn_transport_get_remote_idle_timeout, pn_transport_get_remote_max_frame, \
    pn_transport_get_user, pn_transport_is_authenticated, pn_transport_is_encrypted, pn_transport_log, \
    pn_transport_peek, pn_transport_pending, pn_transport_pop, pn_transport_push, pn_transport_remote_channel_max, \
    pn_transport_require_auth, pn_transport_require_encryption, pn_transport_set_channel_max, \
    pn_transport_set_idle_timeout, pn_transport_set_max_frame, pn_transport_set_server, \
    pn_transport_tick, pn_transport_trace, pn_transport_unbind, \
    PN_CONNECTION_BOUND, PN_CONNECTION_FINAL, PN_CONNECTION_INIT, PN_CONNECTION_LOCAL_CLOSE, \
    PN_CONNECTION_LOCAL_OPEN, PN_CONNECTION_REMOTE_CLOSE, PN_CONNECTION_REMOTE_OPEN, PN_CONNECTION_UNBOUND, PN_DELIVERY, \
    PN_LINK_FINAL, PN_LINK_FLOW, PN_LINK_INIT, PN_LINK_LOCAL_CLOSE, PN_LINK_LOCAL_DETACH, PN_LINK_LOCAL_OPEN, \
    PN_LINK_REMOTE_CLOSE, PN_LINK_REMOTE_DETACH, PN_LINK_REMOTE_OPEN, PN_SESSION_FINAL, PN_SESSION_INIT, \
    PN_SESSION_LOCAL_CLOSE, PN_SESSION_LOCAL_OPEN, PN_SESSION_REMOTE_CLOSE, PN_SESSION_REMOTE_OPEN, PN_TIMER_TASK, \
    PN_TRANSPORT, PN_TRANSPORT_CLOSED, PN_TRANSPORT_ERROR, PN_TRANSPORT_HEAD_CLOSED, PN_TRANSPORT_TAIL_CLOSED, \
    pn_cast_pn_connection, pn_cast_pn_delivery, pn_cast_pn_link, pn_cast_pn_session, pn_cast_pn_transport, \
    pn_class_name, pn_collector, pn_collector_free, pn_collector_more, pn_collector_peek, pn_collector_pop, \
    pn_collector_release, pn_event_class, pn_event_connection, pn_event_context, pn_event_delivery, \
    pn_event_link, pn_event_session, pn_event_transport, pn_event_type, pn_event_type_name, \
    PN_DEFAULT_PRIORITY, PN_STRING, PN_UUID, PN_OVERFLOW, pn_error_text, pn_message, \
    pn_message_annotations, pn_message_body, pn_message_clear, pn_message_decode, \
    pn_message_encode, pn_message_error, pn_message_free, pn_message_get_address, pn_message_get_content_encoding, \
    pn_message_get_content_type, pn_message_get_correlation_id, pn_message_get_creation_time, pn_message_get_delivery_count, \
    pn_message_get_expiry_time, pn_message_get_group_id, pn_message_get_group_sequence, pn_message_get_id, pn_message_get_priority, \
    pn_message_get_reply_to, pn_message_get_reply_to_group_id, pn_message_get_subject, pn_message_get_ttl, \
    pn_message_get_user_id, pn_message_instructions, pn_message_is_durable, \
    pn_message_is_first_acquirer, pn_message_is_inferred, pn_message_properties, pn_message_set_address, \
    pn_message_set_content_encoding, pn_message_set_content_type, pn_message_set_correlation_id, pn_message_set_creation_time, \
    pn_message_set_delivery_count, pn_message_set_durable, pn_message_set_expiry_time, pn_message_set_first_acquirer, \
    pn_message_set_group_id, pn_message_set_group_sequence, pn_message_set_id, pn_message_set_inferred, pn_message_set_priority, \
    pn_message_set_reply_to, pn_message_set_reply_to_group_id, pn_message_set_subject, \
    pn_message_set_ttl, pn_message_set_user_id, \
    PN_EVENT_NONE, \
    pn_create_pyref

def isnull(object):
    return object is None or object == ffi.NULL


def addressof(object):
    return ffi.cast('uint64_t', object)


def pn_py2void(object):
    return ffi.new_handle(object)


def pn_void2py(void):
    return ffi.from_handle(void)


def pn_transport_get_pytracer():
    pass


def pn_transport_set_pytracer():
    pass


PN_PYREF = pn_create_pyref()


def pn_collector_put_pyref(collector, obj, etype):
    lib.pn_collector_put(collector, PN_PYREF, pn_py2void(obj), etype.number)


def pn_class_name(clazz):
    return ffi.string(lib.pn_class_name(clazz)).decode('utf8')


'''
const char *pn_condition_get_description(pn_condition_t *condition);
const char *pn_condition_get_name(pn_condition_t *condition);
int pn_condition_set_description(pn_condition_t *condition, const char *description);
int pn_condition_set_name(pn_condition_t *condition, const char *name);
const char *pn_connection_get_authorization(pn_connection_t *connection);
const char *pn_connection_get_container(pn_connection_t *connection);
const char *pn_connection_get_hostname(pn_connection_t *connection);
const char *pn_connection_get_user(pn_connection_t *connection);
const char *pn_connection_remote_container(pn_connection_t *connection);
const char *pn_connection_remote_hostname(pn_connection_t *connection);
void pn_connection_set_authorization(pn_connection_t *connection, const char *authzid);
void pn_connection_set_container(pn_connection_t *connection, const char *container);
void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname);
void pn_connection_set_password(pn_connection_t *connection, const char *password);
void pn_connection_set_user(pn_connection_t *connection, const char *user);
ssize_t pn_data_decode(pn_data_t *data, const char *bytes, size_t size);
ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size);
int pn_data_format(pn_data_t *data, char *bytes, size_t *size);
_Bool pn_data_lookup(pn_data_t *data, const char *name);
const char *pn_error_text(pn_error_t *error);
const char *pn_event_type_name(pn_event_type_t type);
const char *pn_link_name(pn_link_t *link);
ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n);
ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n);
int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size);
int pn_message_encode(pn_message_t *msg, char *bytes, size_t *size);
const char *pn_message_get_address(pn_message_t *msg);
const char *pn_message_get_content_encoding(pn_message_t *msg);
const char *pn_message_get_content_type(pn_message_t *msg);
const char *pn_message_get_group_id(pn_message_t *msg);
const char *pn_message_get_reply_to(pn_message_t *msg);
const char *pn_message_get_reply_to_group_id(pn_message_t *msg);
const char *pn_message_get_subject(pn_message_t *msg);
int pn_message_set_address(pn_message_t *msg, const char *address);
int pn_message_set_content_encoding(pn_message_t *msg, const char *encoding);
int pn_message_set_content_type(pn_message_t *msg, const char *type);
int pn_message_set_group_id(pn_message_t *msg, const char *group_id);
int pn_message_set_reply_to(pn_message_t *msg, const char *reply_to);
int pn_message_set_reply_to_group_id(pn_message_t *msg, const char *reply_to_group_id);
int pn_message_set_subject(pn_message_t *msg, const char *subject);
pn_link_t *pn_receiver(pn_session_t *session, const char *name);
void pn_sasl_allowed_mechs(pn_sasl_t *sasl, const char *mechs);
void pn_sasl_config_name(pn_sasl_t *sasl, const char *name);
void pn_sasl_config_path(pn_sasl_t *sasl, const char *path);
const char *pn_sasl_get_authorization(pn_sasl_t *sasl);
const char *pn_sasl_get_mech(pn_sasl_t *sasl);
const char *pn_sasl_get_user(pn_sasl_t *sasl);
pn_link_t *pn_sender(pn_session_t *session, const char *name);
int pn_ssl_domain_set_credentials(pn_ssl_domain_t *domain, const char *credential_1, const char *credential_2, const char *password);
int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain, const pn_ssl_verify_mode_t mode, const char *trusted_CAs);
int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain, const char *certificate_db);
int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0, char *fingerprint, size_t fingerprint_length, pn_ssl_hash_alg hash_alg);
_Bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size);
int pn_ssl_get_peer_hostname(pn_ssl_t *ssl, char *hostname, size_t *bufsize);
_Bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size);
const char *pn_ssl_get_remote_subject(pn_ssl_t *ssl);
const char *pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field);
int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_domain_t *domain, const char *session_id);
int pn_ssl_set_peer_hostname(pn_ssl_t *ssl, const char *hostname);
const char *pn_terminus_get_address(pn_terminus_t *terminus);
int pn_terminus_set_address(pn_terminus_t *terminus, const char *address);
const char *pn_transport_get_user(pn_transport_t *transport);
void pn_transport_log(pn_transport_t *transport, const char *message);
ssize_t pn_transport_peek(pn_transport_t *transport, char *dst, size_t size);
ssize_t pn_transport_push(pn_transport_t *transport, const char *src, size_t size);
'''