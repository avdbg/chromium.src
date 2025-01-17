This directory contains the integration between Chromium and the macOS
ScreenTime system, which is a digital wellbeing tool allowing users to restrict
their own use of apps and websites by category.

The ScreenTime system API is documented [on
apple.com](https://developer.apple.com/documentation/screentime?language=objc).
The most pertinent class is `STWebpageController`, which is an
`NSViewController` subclass. Clients of ScreenTime construct a single
`STWebpageController` per tab and splice its corresponding NSView into their
view tree in such a way that it covers the web contents. The NSView becomes
opaque when screen time for that tab or website has been used up.

The public interface to ScreenTime within Chromium is the
`screentime::TabHelper` class, which is a
[TabHelper](../../../../../docs/tab_helpers.md) that binds an
STWebpageController to a WebContents.

## Testing

So that tests can avoid depending on the real ScreenTime system,
STWebpageController is wrapped by a C++ class called
screentime::WebpageController, which has a testing fake called
screentime::FakeWebpageController.
