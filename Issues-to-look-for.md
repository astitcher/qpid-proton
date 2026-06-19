 List/map/array pre-allocation without upper bound checks. The
encoded 'count' being trusted rather than verified as limited <= the
'size' (itself being checked as <= available bytes), and so
potentially being used to pre-allocate large structures to decode
elements into. A related but more nuanced variant of this follows
next.

- Arrays of 0-width elements. Array encodings contain commonly-typed
values encoded after a singular type constructor. The type system
includes '0-width' null, boolean-true/false, uint0, ulong0, and list0
type codes that combine both their constructor encoding code and their
value implicitly. Arrays of such 'values' are not forbidden (though it
would be unusual to encode them, and I'm not sure anything does, due
to the inspection overhead required to do it). Since their constructor
code conveys their value implicitly, this means a small number of
bytes can encode an array requiring a large amount of memory to
decode, as the encoded 'count' (up to 2^32 -1) essentially dictates
that by itself since the 'size' will always be tiny as the element
encodings have no actual width.

- Missing recursion depth limit in type decoder. No bounds on
recursion allows for nested types such that a stack overflow error
eventually occurs.

- Unbounded Symbol cache growth. Allowing either many small entries or
fewer large entries to increasingly fill and occupy memory by filling
the cache. I have since noticed that Symbols are also used in some
SASL frames, so this would apply pre-authentication also.

- Unbounded pre-delivery state growth on incomplete transfer
continuation. Can give increased CPU usage if repeatedly sending
little-or-nothing. No limits on how many transfers can say 'more'
without delivery completing.

- Disposition range loop with unbounded iteration. Dispositions
specify a first and optional last. If values specified vary by a large
amount, excessive CPU could be expended looping and processing the
values determining there is nothing to do.

- Min max frame size not enforced. The spec states the
min-max-frame-size is 512, but upon receiving an Open this isnt
enforced so smaller values could be chosen, causing encoding problems
or increased overhead.

- Session flow-control bypass. The scenario described and addressed
was around the sessions 'remote outgoing-window' and handling when it
hits 0, but I found that to be spurious since the remote controls that
window themselves (its about describing max excess beyond our incoming
window limit), and also I dont know of any component using it in any
functional way. However, in thinking this through more widely since, I
realise there is likely a bypass on the more important session
incoming-window handling, which is controlled by the local side in
order to govern what transfers can be received on the session. The
checking being done is seemingly only in order to re-expand this
window if it ever hits 0, meaning that if it does so and couldnt be
expanded, any unexpected further incoming transfers beyond the
previously advertised window are still going to be permitted.

- Unbounded 'echo' flow responses. A flow frame can request an 'echo'
of state at the session or link level, and prompt a response flow
frame. There is no limit on the amount of echo flows the broker will
process and respond to, causing CPU and bandwidth usage to send
responses. I dont believe proton supports 'echo' flows.
