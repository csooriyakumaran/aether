# Design: Windows + OpenGL Platform Layer (scoped to hello-triangle → spinning cube)

Status: DRAFT (2026-10-17). Scoped narrowly on purpose — this is *not* the
full eventual graphics platform layer named back at the start of the vec
math work. It's just enough to get a window, a modern GL context, and one
spinning cube on screen, driven by `aether`'s `vec3`/`vec4`/`mat4`.

Consumer driving the requirements: a hello-triangle example first (pure
platform/GL-pipeline sanity check — no transform math needed), immediately
followed by a spinning cube (the actual exercise: model/view/projection
matrices, composed and applied per-vertex via `mat4_mul_vec4`). Per this
project's own precedent (`iris`, `hermes`/`cereal`), the code starts living
*inside the example*, not as a new header — promotion to a real module
(mythology-named, matching `iris`/`hermes`) is a later decision once a
second consumer, or a stable enough shape, justifies it. Pruning pieces of
the example into reusable code as the shape becomes clear is the explicit
plan, not an afterthought.

## Requirements

| # | requirement |
|---|-------------|
| R1 | Create a Win32 window; report resize and close requests to the caller |
| R2 | Create a modern (core-profile) OpenGL context on that window |
| R3 | Load the OpenGL functions beyond 1.1 actually needed to compile a shader, build a vertex buffer, and draw (not a general-purpose loader) |
| R4 | A per-frame loop with delta-time, built on `aether`'s existing `time_mark`/`time_elapsed_sec` — not a new timer |
| R5 | A minimal shader-compile-and-link helper: GLSL source in, program handle + pass/fail out |
| R6 | Windows-only, matching this project's current platform scope everywhere else |
| R7 | Minimal dependencies — no GLAD/GLEW/GLFW |

## Scope

Window + GL context creation, the small fixed set of GL function pointers
R3 needs, the per-frame loop, and the shader helper. That's the entire
scope — enough to compile and run hello-triangle, then the cube.

**Non-goals (this pass):**
- Any renderer abstraction (render passes, materials, draw-call batching,
  multiple shaders/pipelines) — far premature for one triangle and one
  cube.
- Input handling beyond "window wants to close" / "window was resized" —
  no keyboard/mouse/gamepad yet, nothing needs it.
- Multiple windows, fullscreen toggle, monitor/DPI handling.
- Cross-platform (Linux/macOS GL, Vulkan, or any abstraction over "which
  graphics API") — Windows + OpenGL only, matching the rest of this
  project.
- Asset loading (meshes, textures, OBJ/glTF parsers). The cube's 8
  vertices are hand-written inline; no loader needed until a real asset
  shows up (this is exactly why last turn ruled out the Utah teapot as a
  first example — it would have pulled this non-goal into scope early for
  no reason).
- A general-purpose OpenGL function loader (the GLAD/GLEW shape) — R3 is
  deliberately a short, named list, not "load everything the driver
  exposes."
- Deciding the eventual module's mythology name, or promoting this code
  out of the example at all — stays inside the example until a second
  consumer or a settled shape argues otherwise, per `iris`/`hermes`'s own
  precedent.

## Open decisions — not yet settled, need your call before implementation

Unlike the vec math doc at this stage, these haven't been talked through
yet, so they're listed as forks rather than resolved decisions.

1. **GL version/profile target.** `3.3 core` is the conservative default —
   released 2010, broadly supported down to fairly old integrated GPUs,
   and has everything R1–R5 need (shaders, VAOs/VBOs, `glDrawArrays`).
   `4.5`+ core adds direct-state-access functions (`glCreateBuffers`
   instead of `glGenBuffers`+`glBindBuffer`, etc.) that trim some
   boilerplate out of the helper code, at the cost of narrowing hardware/
   driver support somewhat further. Given this project's own "simple,
   minimal" taste, I'd lean `3.3 core` as the default unless you have a
   specific reason to want the newer API surface — your call.

2. **Hand-rolled GL function loader vs. a dependency (GLAD/GLEW).**
   R3/R7 already lean hand-rolled — this mirrors `iris`'s own precedent
   exactly (`WS2API`, a private table of `GetProcAddress`-resolved
   WinSock function pointers, cached once at `net_init`, specifically to
   avoid a hard link dependency). The GL equivalent is the same shape:
   `wglGetProcAddress`-resolved pointers for the handful of functions R3
   names, resolved once at context-creation time. I think this is the
   obvious extension of already-established practice here, not really a
   coin-flip — flagging it as a decision mainly so it's explicit, not
   because I think GLAD is a close second.

3. **The WGL context-creation bootstrap is inherently two-step, and
   that's not optional.** Not really a decision — more a shape you should
   know about before starting, since it surprises people new to Win32+GL:
   getting a *modern* (core-profile) context requires
   `wglCreateContextAttribsARB`, which is itself a WGL extension function
   — and extension functions can only be resolved via `wglGetProcAddress`
   once *some* GL context is already current. So step one is an
   almost-immediately-discarded legacy context (just to make
   `wglGetProcAddress` work at all), step two destroys it and creates the
   real one. Noting this now so it doesn't look like a design mistake
   when it shows up in the implementation.

## Usage sketch (illustrative, not a locked API)

```c
GfxWindow win = gfx_window_open(STR("aether cube"), 1280, 720);
GLuint    prog = gfx_shader_compile(vs_src, fs_src);  /* R5 */

u64 t_prev = time_mark();
while (!gfx_window_should_close(&win))
{
    gfx_window_pump(&win);   /* R1: message loop, resize/close detection */

    u64 t_now = time_mark();
    f32 dt = (f32)time_elapsed_sec(t_prev, t_now);   /* R4 */
    t_prev = t_now;

    mat4 model = mat4_rotate(/* axis, angle accumulated by dt */);
    mat4 mvp   = mat4_mul(proj, mat4_mul(view, model));

    glUseProgram(prog);
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp.e);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    gfx_window_swap(&win);
}
```

## Follow-ups after this lands

- `mat4_rotate`/`mat4_perspective`/`mat4_look_at` (drafted, not designed,
  in `docs/vec-math-design.md`'s mat4/quat section) need to actually get
  designed and implemented — the cube can't rotate without at least one
  of them.
- Decide the eventual module name/promotion once the example's shape
  settles.
- The batched-transform-API follow-up already named in
  `docs/vec-math-design.md` (decision 15) becomes relevant again if this
  ever grows past one cube's worth of vertices.
