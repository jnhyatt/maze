# What the heck?

There I was, sitting around thinking about coding up some fun graphics application with Vulkan or something, when suddenly my early graphics programming memories came back to me. How simple it had all been, back when I could simply `glutCreateWindow`, then `glBegin(GL_TRIANGLES)` straight into it. The days before `VkPipelineShaderStageCreateInfo`. Back when `VkAccelerationStructureGeometryKHR` wasn't even in my vocabulary. Those blissful days devoid of any `vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV`.

I wondered if GLUT still worked. Surely by now something somewhere has to have broken that renders GLUT programs nonfunctional? Code doesn't just live that long. Not mine, at least. If I leave it sitting around, it starts to smell.

To my shock, GLUT programs still work just fine! Of course, there's no reason they shouldn't, but it was still a surprise. A welcome one. I got right to work remaking that one screensaver from Windows 95 or thereabouts -- the nostalgia must've hit in several places at once.

I picked a nice, simple maze generation algorithm and went to implement it -- but wait, that's going to get yucky if I write it in C -- I simply despise mucking about with dependencies without a proper library ecosystem. No standardization, and nothing built in.

So it was that I decided to explore Rust FFI -- I seldom find myself needing it, but this little project presented me with an opportunity to learn something new, and to avoid writing C code I wouldn't be able to read in two weeks (instead I wrote Rust code I won't be able to read in two weeks -- all hail the borrow checker).

And it all culminated in this... concoction. I'm discovering I write C the way I write Rust, and C is not meant to be written like that, I don't think. Ah, well. Nothing for it but to crank the maze dimensions up to 400x400 and watch OpenGL 1 struggle to render the sheer glory of it at anywhere near realtime framerates.

## Attribution

Thanks to [alpha_rats](http://alpharats.com) for the beautiful [brick texture](https://opengameart.org/content/bricks-tiled-texture-64x64)! It was the only one I could find that wasn't either too realistic or too 8-bit. Nothing else properly captured the feeling that decades-old screensaver gave me.
