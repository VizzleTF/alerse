// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_webkit/alerse_webkit_host_mac.h"

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <array>

#include "base/containers/span.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "crypto/hash.h"
#include "net/base/apple/url_conversions.h"

// Keeps the WebKit view the size of the tab, less the bar, and reports its
// title, address and loading to the page.
@interface AlerseWebKitDelegate
    : NSObject <WKNavigationDelegate, WKUIDelegate>
- (instancetype)initWithChanged:(AlerseWebKitHost::Changed)changed
                       topInset:(CGFloat)topInset;
- (void)attachTo:(WKWebView*)webView inside:(NSView*)parent;
- (void)detach;
@end

@implementation AlerseWebKitDelegate {
  AlerseWebKitHost::Changed _changed;
  CGFloat _topInset;
  WKWebView* __weak _webView;
  NSView* __weak _parent;
}

- (instancetype)initWithChanged:(AlerseWebKitHost::Changed)changed
                       topInset:(CGFloat)topInset {
  if ((self = [super init])) {
    _changed = std::move(changed);
    _topInset = topInset;
  }
  return self;
}

- (void)attachTo:(WKWebView*)webView inside:(NSView*)parent {
  _webView = webView;
  _parent = parent;
  parent.postsFrameChangedNotifications = YES;
  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(layout)
             name:NSViewFrameDidChangeNotification
           object:parent];
  for (NSString* key in @[ @"title", @"URL", @"canGoBack", @"loading" ]) {
    [webView addObserver:self forKeyPath:key options:0 context:nil];
  }
  [self layout];
}

- (void)detach {
  [NSNotificationCenter.defaultCenter removeObserver:self];
  WKWebView* webView = _webView;
  for (NSString* key in @[ @"title", @"URL", @"canGoBack", @"loading" ]) {
    [webView removeObserver:self forKeyPath:key];
  }
}

- (void)layout {
  NSView* parent = _parent;
  WKWebView* webView = _webView;
  if (!parent || !webView) {
    return;
  }
  NSRect bounds = parent.bounds;
  CGFloat height = MAX(0, NSHeight(bounds) - _topInset);
  webView.frame = NSMakeRect(
      NSMinX(bounds), parent.isFlipped ? NSMinY(bounds) + _topInset
                                       : NSMinY(bounds),
      NSWidth(bounds), height);
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  WKWebView* webView = _webView;
  if (!webView) {
    return;
  }
  AlerseWebKitHost::State state;
  state.title = base::SysNSStringToUTF16(webView.title ?: @"");
  state.url = net::GURLWithNSURL(webView.URL);
  state.can_go_back = webView.canGoBack;
  state.loading = webView.loading;
  _changed.Run(state);
}

// A link that opens a window opens here: the page has one view.
- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)navigationAction
                    windowFeatures:(WKWindowFeatures*)windowFeatures {
  if (navigationAction.request.URL) {
    [webView loadRequest:navigationAction.request];
  }
  return nil;
}

@end

struct AlerseWebKitHost::Views {
  WKWebView* __strong web_view;
  AlerseWebKitDelegate* __strong delegate;
};

namespace {

// The Space's own WebKit storage: an identifier made from its profile's
// path, so that it stays the same across starts.
WKWebsiteDataStore* DataStoreFor(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return [WKWebsiteDataStore nonPersistentDataStore];
  }
  if (@available(macOS 14.0, *)) {
    const std::array<uint8_t, crypto::hash::kSha256Size> digest =
        crypto::hash::Sha256(base::as_byte_span(profile->GetPath().value()));
    NSUUID* uuid = [[NSUUID alloc] initWithUUIDBytes:digest.data()];
    return [WKWebsiteDataStore dataStoreForIdentifier:uuid];
  }
  return [WKWebsiteDataStore defaultDataStore];
}

}  // namespace

AlerseWebKitHost::AlerseWebKitHost(content::WebContents* contents,
                                   const GURL& url,
                                   int top_inset,
                                   Changed changed)
    : views_(std::make_unique<Views>()) {
  NSView* parent = contents->GetNativeView().GetNativeNSView();
  WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
  config.websiteDataStore = DataStoreFor(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  config.preferences.elementFullscreenEnabled = YES;
  config.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
  // Streaming services serve FairPlay to Safari, and look for it by name.
  config.applicationNameForUserAgent = @"Version/18.5 Safari/605.1.15";

  WKWebView* web_view = [[WKWebView alloc] initWithFrame:parent.bounds
                                           configuration:config];
  web_view.allowsBackForwardNavigationGestures = YES;
  web_view.allowsMagnification = YES;
  AlerseWebKitDelegate* delegate =
      [[AlerseWebKitDelegate alloc] initWithChanged:std::move(changed)
                                           topInset:top_inset];
  web_view.navigationDelegate = delegate;
  web_view.UIDelegate = delegate;
  [parent addSubview:web_view positioned:NSWindowAbove relativeTo:nil];
  [delegate attachTo:web_view inside:parent];
  [web_view loadRequest:[NSURLRequest requestWithURL:net::NSURLWithGURL(url)]];
  views_->web_view = web_view;
  views_->delegate = delegate;
}

AlerseWebKitHost::~AlerseWebKitHost() {
  [views_->delegate detach];
  [views_->web_view stopLoading];
  [views_->web_view removeFromSuperview];
}

void AlerseWebKitHost::GoBack() {
  [views_->web_view goBack];
}

void AlerseWebKitHost::Reload() {
  [views_->web_view reload];
}

void AlerseOpenInSafari(const GURL& url) {
  NSURL* safari = [NSWorkspace.sharedWorkspace
      URLForApplicationWithBundleIdentifier:@"com.apple.Safari"];
  if (!safari) {
    return;
  }
  [NSWorkspace.sharedWorkspace openURLs:@[ net::NSURLWithGURL(url) ]
                   withApplicationAtURL:safari
                          configuration:[NSWorkspaceOpenConfiguration
                                            configuration]
                      completionHandler:nil];
}
