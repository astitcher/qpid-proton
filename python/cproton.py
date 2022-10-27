from cproton_ffi import lib
from cproton_ffi import ffi
from cproton_ffi.lib import *

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