# EL0 IPC data access

Syscall 14 uses x0=port, x1=payload, x2=length, x3=broker capability ID.
Syscall 15 uses x0=port, x1=output buffer, x2=capacity, x3=broker capability ID.
The port owner may pass zero in x3. A non-owner must provide a valid ID for the same task, port, and requested SEND or RECEIVE right. Invalid credentials return -13 before queue access.

Receive validates the full output range before dequeuing. Revocation blocks future operations; already queued messages remain available to the owner. Task exit removes holder grants before the task identity is released.
