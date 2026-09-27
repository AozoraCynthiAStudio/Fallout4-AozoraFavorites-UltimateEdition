package aozora.favorites
{
    import flash.display.Bitmap;
    import flash.display.DisplayObject;
    import flash.display.Graphics;
    import flash.display.Loader;
    import flash.display.LoaderInfo;
    import flash.display.MovieClip;
    import flash.display.Shape;
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.IOErrorEvent;
    import flash.events.MouseEvent;
    import flash.geom.Point;
    import flash.geom.ColorTransform;
    import flash.net.URLRequest;
    import flash.system.ApplicationDomain;
    import flash.text.TextField;
    import flash.text.TextFormat;
    import flash.utils.clearTimeout;
    import flash.utils.setTimeout;

    /**
     * skyui like v1n visual layer.
     *
     * The panel, rows, tabs, scroll rail, hints, scanlines and selection frame
     * are drawn at runtime. External textures are limited to the optional
     * mascot and the quiet background silhouette.
     */
    public class SkyuiLikeMenuPanel extends MovieClip
    {
        public var BGSCodeObj:Object = {};

        private var backgroundLayer:Sprite;
        private var silhouetteLayer:Sprite;
        private var contentLayer:Sprite;
        private var themeLayer:Sprite;
        private var rowLayer:Sprite;
        private var mascotLayer:Sprite;
        private var mascotBackdrop:Bitmap;
        private var scanlineLayer:Sprite;
        private var mascotController:SkyuiLikeLookController;
        private var silhouetteBitmap:Bitmap;
        private var f4seCodeObject:Object;
        private var iconLibraryLoader:Loader;
        private var iconLibraryDomain:ApplicationDomain;
        private var iconLibraryReady:Boolean = false;
        private var iconLibraryRetry:Boolean = false;
        private var iconLibraryLoaders:Object = {};
        private var iconLibraryDomains:Object = {};
        private var iconLibraryRetries:Object = {};
        private var iconLibraryRetryTimers:Object = {};
        private var iconDiagnosticKeys:Object = {};
        private var lastEquipmentDiagnostic:String = "";
        private var silhouetteMounted:Boolean = false;
        private var silhouetteId:String = "";
        private static var lastSilhouetteIndex:int = -1;
        private var editorElapsed:Number = 0.0;

        private var layout:Object = {};
        private var theme:Object = { r: 0.40, g: 1.0, b: 0.70 };
        private var items:Array = [];
        private var visibleItems:Array = [];
        private var rows:Array = [];
        private var tabs:Array = [];
        private var footerKeys:Array = [];
        private var footerLabels:Array = [];
        private var selectedIndex:int = 0;
        private var scrollStart:int = 0;
        private var hoverIndex:int = -1;
        private var hoverCategoryIndex:int = -1;
        private var nameMarqueeSignature:String = "";
        private var nameMarqueePhase:int = 0;
        private var nameMarqueeTimer:Number = 0.0;
        private var nameMarqueeOffset:Number = 0.0;
        private var categoryIndex:int = 0;
        private var focusSource:int = 0;
        private var iconMode:int = 2;
        private var showItemInnerName:Boolean = false;
        private var mascotEnabled:Boolean = true;
        private var lastStageDiagnostic:String = "";
        private var focusSignature:String = "";
        private var hasFocusSignature:Boolean = false;
        private var mouseMovedSinceInput:Boolean = false;
        private var hasMousePosition:Boolean = false;
        private var lastMouseStageX:Number = 0.0;
        private var lastMouseStageY:Number = 0.0;
        private var statusField:TextField;

        private const categories:Array = ["ALL", "WEAP", "ARMO", "ALCH"];
        private const categoryLabels:Array = [
            "$AOZORA_CATEGORY_ALL", "$AOZORA_CATEGORY_WEAPONS",
            "$AOZORA_CATEGORY_OUTFITS", "$AOZORA_CATEGORY_AID"];
        private const BASE_STAGE_WIDTH:Number = 1280.0;
        private const BASE_STAGE_HEIGHT:Number = 720.0;
        private var focusColor:uint = 0xFFC857;
        private var equippedColor:uint = 0x65E7FF;
        private const fallbackWidth:Number = 322.0;
        private const fallbackHeight:Number = 560.0;

        public function SkyuiLikeMenuPanel()
        {
            super();
            mouseEnabled = true;
            mouseChildren = true;
            BGSCodeObj.ProcessUserEvent = ProcessUserEvent;

            backgroundLayer = new Sprite();
            silhouetteLayer = new Sprite();
            contentLayer = new Sprite();
            themeLayer = new Sprite();
            rowLayer = new Sprite();
            mascotLayer = new Sprite();
            scanlineLayer = new Sprite();
            backgroundLayer.mouseEnabled = false;
            silhouetteLayer.mouseEnabled = false;
            scanlineLayer.mouseEnabled = false;
            addChild(backgroundLayer);
            addChild(silhouetteLayer);
            addChild(contentLayer);
            addChild(mascotLayer);
            contentLayer.addChild(themeLayer);
            contentLayer.addChild(rowLayer);
            contentLayer.addChild(scanlineLayer);

            mascotBackdrop = new Bitmap();
            mascotBackdrop.smoothing = true;
            mascotLayer.addChild(mascotBackdrop);
            mascotController = new SkyuiLikeLookController();
            mascotController.onReady = onMascotReady;
            mascotController.onError = onMascotError;
            mascotLayer.addChild(mascotController);

            layout = defaultLayout();
            addEventListener(MouseEvent.MOUSE_MOVE, onMouseMove, true);
            addEventListener(MouseEvent.MOUSE_OUT, onMouseOut, true);
            addEventListener(MouseEvent.MOUSE_WHEEL, onMouseWheel, true);
            addEventListener(Event.ENTER_FRAME, animateVisuals);
            rebuildVisual();
        }

        public function InitializeAfterNativeBinding():void
        {
            if (BGSCodeObj && BGSCodeObj.Initialize is Function)
                BGSCodeObj.Initialize();
            loadIconLibrary();
            findF4SECodeObject();
        }

        public function OnF4SEObjCreated(codeObject:Object):void
        {
            if (!codeObject || !(codeObject.MountImage is Function))
                return;
            f4seCodeObject = codeObject;
            mountExternalAssets();
        }

        public function ApplySnapshot(snapshot:Object):void
        {
            items = snapshot && snapshot.items is Array ? snapshot.items as Array : [];
            diagnoseEquipmentSnapshot();
            if (snapshot && snapshot.categoryIndex is Number) {
                var nextCategory:int = clampInt(int(snapshot.categoryIndex), 0, categories.length - 1);
                if (nextCategory != categoryIndex) {
                    scrollStart = 0;
                    hoverIndex = -1;
                    hoverCategoryIndex = -1;
                }
                categoryIndex = nextCategory;
            }
            if (snapshot && snapshot.selectedIndex is Number)
                selectedIndex = Math.max(0, int(snapshot.selectedIndex));
            if (snapshot && snapshot.focusSource is Number)
                focusSource = clampInt(int(snapshot.focusSource), 0, 3);
            if (snapshot && snapshot.iconMode is Number)
                iconMode = clampInt(int(snapshot.iconMode), 0, 2);
            if (snapshot && snapshot.showItemInnerName is Boolean)
                showItemInnerName = Boolean(snapshot.showItemInnerName);
            if (snapshot && snapshot.mascotEnabled is Boolean)
                mascotEnabled = Boolean(snapshot.mascotEnabled);
            if (snapshot && snapshot.theme is Object)
                theme = snapshot.theme;
            updateFocusColor();
            if (snapshot && snapshot.layout is Object) {
                var current:Object = snapshot.layout.current;
                if (current is Object)
                    layout = current;
                editorElapsed = 0.0;
            }

            filterVisibleItems();
            var nextFocus:String = currentFocusSignatureFor(visibleItems);
            var focusChanged:Boolean = nextFocus.length > 0 &&
                hasFocusSignature && nextFocus != focusSignature;
            if (nextFocus.length > 0) {
                focusSignature = nextFocus;
                hasFocusSignature = true;
            } else {
                // An empty category has no focused item. Clear the previous
                // signature so every refresh does not advance the mascot.
                focusSignature = "";
                hasFocusSignature = false;
            }
            if (focusChanged && mascotController)
                mascotController.onFocusChanged();

            rebuildVisual();
            if (focusSource != 3)
            {
                mouseMovedSinceInput = false;
                // Ignore the synthetic mouse move generated when the panel
                // appears under an already-positioned cursor. A real move
                // after this baseline can still take focus from the keyboard.
                if (stage) {
                    lastMouseStageX = stage.mouseX;
                    lastMouseStageY = stage.mouseY;
                    hasMousePosition = true;
                }
            }
        }

        public function UpdateSelectionOnly(nextIndex:int, nextFocusSource:int):Boolean
        {
            if (nextIndex < 0 || nextIndex >= visibleItems.length)
                return false;
            if (!rowLayer || !rowLayer.getChildByName("favoriteRow_" + nextIndex))
                return false;

            var normalizedFocus:int = clampInt(nextFocusSource, 0, 3);
            // The footer keycaps differ between keyboard/mouse and gamepad.
            // Let ApplySnapshot rebuild once when crossing that boundary.
            if ((focusSource == 2) != (normalizedFocus == 2))
                return false;

            selectedIndex = nextIndex;
            focusSource = normalizedFocus;
            var nextFocus:String = currentFocusSignatureFor(visibleItems);
            if (nextFocus.length > 0) {
                if (hasFocusSignature && nextFocus != focusSignature && mascotController)
                    mascotController.onFocusChanged();
                focusSignature = nextFocus;
                hasFocusSignature = true;
            } else {
                focusSignature = "";
                hasFocusSignature = false;
            }

            updateSelection();
            applyNameMarqueePosition();
            if (focusSource != 3) {
                mouseMovedSinceInput = false;
                if (stage) {
                    lastMouseStageX = stage.mouseX;
                    lastMouseStageY = stage.mouseY;
                    hasMousePosition = true;
                }
            }
            return true;
        }

        public function ApplyStatus(status:String):void
        {
            if (!statusField)
                return;
            statusField.text = status || "";
            statusField.visible = statusField.text.length > 0;
        }

        public function ProcessUserEvent(userEvent:String, isDown:Boolean):void
        {
            // Input is owned by the native menu instance. This callback remains
            // present for the Scaleform contract but must not duplicate input.
        }

        private function defaultLayout():Object
        {
            var value:Object = {};
            value.panelX = 36.0; value.panelY = 78.0; value.panelScale = 1.0;
            value.width = fallbackWidth; value.height = fallbackHeight;
            value.contentX = 0.0; value.contentY = 0.0;
            value.contentScaleX = 1.0; value.contentScaleY = 1.0;
            value.titleBlockX = 0.0; value.titleBlockY = 0.0;
            value.categoryBlockX = 0.0; value.categoryBlockY = 0.0;
            value.headerBlockX = 0.0; value.headerBlockY = 0.0;
            value.listBlockX = 0.0; value.listBlockY = 0.0;
            value.footerBlockX = 0.0; value.footerBlockY = 0.0;
            value.titleX = 16.0; value.titleY = 18.0; value.titleFontSize = 29.0;
            value.titleLetterSpacing = 0.6;
            value.categoryX = 16.0; value.categoryY = 67.0;
            value.categoryStep = 66.0; value.categoryWidth = 54.0;
            value.categoryFontSize = 18.0; value.categoryLetterSpacing = 0.0;
            value.categoryHitX = 0.0; value.categoryHitY = -4.0;
            value.categoryHitWidth = 66.0; value.categoryHitHeight = 34.0;
            value.headerY = 108.0; value.headerNameX = 47.0;
            value.headerHotkeyX = 210.0; value.headerQuantityX = 259.0;
            value.headerFontSize = 12.0; value.headerLetterSpacing = 0.0;
            value.listX = 12.0; value.listTop = 126.0;
            value.rowHeight = 45.0; value.rowWidth = 296.0;
            value.rowIconX = 13.0; value.rowIconY = 10.0; value.rowIconScale = 1.0;
            value.equippedRowAlpha = 0.10;
            value.rowTextBlockY = 0.0;
            value.rowNameX = 47.0; value.rowNameY = 7.0;
            value.rowNameWidth = 150.0; value.rowFontSize = 17.0;
            value.rowLetterSpacing = 0.0;
            value.nameScrollDelay = 0.80; value.nameScrollSpeed = 24.0;
            value.nameScrollEndPause = 0.90;
            value.focusFrameX = 0.0; value.focusFrameY = 0.0;
            value.focusFrameWidth = 296.0; value.focusFrameHeight = 42.0;
            value.focusFrameThickness = 1.0; value.focusFrameAlpha = 0.98;
            value.rowHotkeyX = 210.0; value.rowHotkeyY = 7.0;
            value.rowHotkeyWidth = 30.0; value.rowHotkeyHeight = 29.0;
            value.rowHotkeyScale = 1.0; value.rowHotkeyFontSize = 16.0;
            value.rowHotkeyLetterSpacing = 0.2;
            value.rowQuantityX = 257.0; value.rowQuantityY = 8.0;
            value.rowQuantityWidth = 39.0; value.rowQuantityFontSize = 15.5;
            value.rowQuantityLetterSpacing = 0.2;
            value.scrollbarX = 307.0; value.scrollbarY = 128.0;
            value.scrollbarWidth = 4.0; value.scrollbarHeight = 360.0;
            value.scrollbarThumbMinHeight = 26.0;
            value.footerX = 16.0; value.footerY = 510.0;
            value.footerWidth = 290.0; value.footerHeight = 34.0;
            value.footerKey1X = 0.0; value.footerKey1Y = 0.0;
            value.footerKey2X = 151.0; value.footerKey2Y = 0.0;
            value.footerKey1TextY = 1.0; value.footerKey2TextY = 1.0;
            value.footerLabel1X = 36.0; value.footerLabel1Y = 5.0;
            value.footerLabel2X = 187.0; value.footerLabel2Y = 5.0;
            value.footerDividerX = 129.0; value.footerDividerY = 1.0;
            value.footerDividerHeight = 28.0;
            value.footerKeyGap = 12.0; value.footerKeyWidth = 28.0;
            value.footerKeyHeight = 34.0; value.footerKeyScale = 1.0;
            value.footerKeyFontSize = 16.0; value.footerKeyLetterSpacing = 0.2;
            value.footerLabelFontSize = 15.0; value.footerLabelLetterSpacing = 0.0;
            value.topMicroFontSize = 8.0; value.topMicroLetterSpacing = 1.8; value.topMicroAlpha = 0.55;
            value.footerDetailFontSize = 7.0; value.footerDetailLetterSpacing = 1.0;
            value.footerDetailAlpha = 0.55;
            value.borderAlpha = 0.86;
            value.outerHorizontalAlpha = 0.28; value.outerVerticalAlpha = 0.42;
            value.innerHorizontalAlpha = 0.75; value.innerVerticalAlpha = 0.66;
            value.frameCornerRadius = 8.0;
            value.backgroundAlpha = 0.88;
            value.rowAlpha = 0.22; value.dividerAlpha = 0.36;
            value.scanlineAlpha = 0.045; value.silhouetteAlpha = 0.035;
            value.silhouetteX = 150.0; value.silhouetteY = 230.0; value.silhouetteScale = 0.28;
            value.mascotBackdropX = 255.0; value.mascotBackdropY = 66.0;
            value.mascotBackdropScale = 0.70; value.mascotBackdropAlpha = 0.14;
            value.mascots = [];
            for (var i:int = 0; i < 5; ++i)
                value.mascots.push({ group: ["Chat", "Snack", "Mechanic", "Explorer", "Groom"][i], x: 230.0, y: -42.0, scale: 0.255, rotation: 0.0, alpha: 0.96 });
            return value;
        }

        private function rebuildVisual():void
        {
            diagnoseStageViewport();
            var viewportWidth:Number = stage && stage.stageWidth > 0 ? stage.stageWidth : BASE_STAGE_WIDTH;
            var viewportHeight:Number = stage && stage.stageHeight > 0 ? stage.stageHeight : BASE_STAGE_HEIGHT;
            var responsiveScale:Number = Math.min(
                viewportWidth / BASE_STAGE_WIDTH,
                viewportHeight / BASE_STAGE_HEIGHT);
            responsiveScale = Math.max(0.05, responsiveScale);

            // The existing 16:9 INI values are authored in the observed
            // 1280x720 Scaleform space. Keep the panel anchored to the left,
            // scale it uniformly, and place Y by relative viewport height.
            x = numberValue(layout, "panelX", 36.0) * responsiveScale;
            y = viewportHeight *
                (numberValue(layout, "panelY", 78.0) / BASE_STAGE_HEIGHT);
            scaleX = numberValue(layout, "panelScale", 1.0) * responsiveScale;
            scaleY = scaleX;
            contentLayer.x = numberValue(layout, "contentX", 0.0);
            contentLayer.y = numberValue(layout, "contentY", 0.0);
            contentLayer.scaleX = numberValue(layout, "contentScaleX", 1.0);
            contentLayer.scaleY = numberValue(layout, "contentScaleY", 1.0);

            var width:Number = numberValue(layout, "width", fallbackWidth);
            var height:Number = numberValue(layout, "height", fallbackHeight);
            clearThemeLayer();
            clearRows();
            drawBackground(width, height);
            drawSilhouette();
            drawScanlines(width, height);
            createStaticFields(width, height);
            rebuildRows();
            updateMascotBackdrop();
            updateMascotLayout();
        }

        private function diagnoseStageViewport():void
        {
            if (!stage || !BGSCodeObj || !(BGSCodeObj.WriteLog is Function))
                return;
            var signature:String = int(stage.stageWidth) + "x" + int(stage.stageHeight) +
                " root=" + int(root.stage.stageWidth) + "x" + int(root.stage.stageHeight);
            if (signature == lastStageDiagnostic)
                return;
            lastStageDiagnostic = signature;
            BGSCodeObj.WriteLog("SWF_STAGE " + signature);
        }

        private function clearThemeLayer():void
        {
            while (themeLayer.numChildren > 0)
                themeLayer.removeChildAt(0);
            tabs = [];
            footerKeys = [];
            footerLabels = [];
            statusField = null;
        }

        private function clearRows():void
        {
            while (rowLayer.numChildren > 0)
                rowLayer.removeChildAt(0);
            rows = [];
        }

        private function drawBackground(width:Number, height:Number):void
        {
            while (backgroundLayer.numChildren > 0)
                backgroundLayer.removeChildAt(0);
            var panel:Shape = new Shape();
            var g:Graphics = panel.graphics;
            fillRect(g, 0x020708, numberValue(layout, "backgroundAlpha", 0.88), 0, 0, width, height);
            drawOriginalFrame(g, width, height);
            backgroundLayer.addChild(panel);
        }

        private function drawOriginalFrame(g:Graphics, width:Number, height:Number):void
        {
            var horizontalAlpha:Number = numberValue(layout, "innerHorizontalAlpha", 0.75);
            var outerAlpha:Number = numberValue(layout, "outerHorizontalAlpha", 0.28);

            // Vanilla-style rails: only the top and bottom bars are drawn.
            // Each end follows a small adjustable quarter-arc into the panel
            // edge; there are no square hooks and no connecting side edges.
            var innerInset:Number = 2.5;
            var radius:Number = numberValue(layout, "frameCornerRadius", 8.0);
            radius = Math.max(2.0, Math.min(radius, Math.min(width, height) * 0.5));
            var innerRadius:Number = Math.max(2.0, radius - innerInset);

            drawRoundedHorizontalRails(g, width, height, radius, 0.0, 1.0,
                themeColor(0.72), outerAlpha);
            drawRoundedHorizontalRails(g, width, height, innerRadius, innerInset, 2.0,
                themeColor(1.0), horizontalAlpha);
        }

        private function drawRoundedHorizontalRails(g:Graphics, width:Number, height:Number,
                radius:Number, inset:Number, thickness:Number, color:uint, alpha:Number):void
        {
            var left:Number = inset;
            var right:Number = width - inset;
            var top:Number = inset;
            var bottom:Number = height - inset;

            g.lineStyle(thickness, color, alpha);
            g.moveTo(left, top + radius);
            g.curveTo(left, top, left + radius, top);
            g.lineTo(right - radius, top);
            g.curveTo(right, top, right, top + radius);

            g.moveTo(left, bottom - radius);
            g.curveTo(left, bottom, left + radius, bottom);
            g.lineTo(right - radius, bottom);
            g.curveTo(right, bottom, right, bottom - radius);
        }

        private function drawSilhouette():void
        {
            while (silhouetteLayer.numChildren > 0)
                silhouetteLayer.removeChildAt(0);
            if (!silhouetteBitmap)
                return;
            silhouetteBitmap.x = numberValue(layout, "silhouetteX", 150.0);
            silhouetteBitmap.y = numberValue(layout, "silhouetteY", 230.0);
            silhouetteBitmap.scaleX = numberValue(layout, "silhouetteScale", 0.28);
            silhouetteBitmap.scaleY = silhouetteBitmap.scaleX;
            var silhouetteAlpha:Number = numberValue(layout, "silhouetteAlpha", 0.035);
            silhouetteBitmap.alpha = 1.0;
            silhouetteBitmap.transform.colorTransform = new ColorTransform(
                0, 0, 0, silhouetteAlpha, themeComponent("r") * 255, themeComponent("g") * 255,
                themeComponent("b") * 255, 0);
            silhouetteLayer.addChild(silhouetteBitmap);
        }

        private function updateMascotBackdrop():void
        {
            if (!mascotBackdrop)
                return;
            var alpha:Number = numberValue(layout, "mascotBackdropAlpha", 0.14);
            mascotBackdrop.visible = mascotBackdrop.bitmapData != null && alpha > 0.0;
            if (!mascotBackdrop.visible)
                return;
            mascotBackdrop.x = numberValue(layout, "mascotBackdropX", 255.0);
            mascotBackdrop.y = numberValue(layout, "mascotBackdropY", 66.0);
            mascotBackdrop.scaleX = numberValue(layout, "mascotBackdropScale", 0.70);
            mascotBackdrop.scaleY = mascotBackdrop.scaleX;
            mascotBackdrop.alpha = alpha;
        }

        private function drawScanlines(width:Number, height:Number):void
        {
            scanlineLayer.graphics.clear();
            scanlineLayer.x = 0;
            scanlineLayer.y = 0;
            scanlineLayer.graphics.lineStyle(1, themeColor(1.0), numberValue(layout, "scanlineAlpha", 0.045));
            for (var yLine:Number = 3.0; yLine < height; yLine += 4.0) {
                scanlineLayer.graphics.moveTo(0, yLine);
                scanlineLayer.graphics.lineTo(width, yLine);
            }
        }

        private function createStaticFields(width:Number, height:Number):void
        {
            var titleBlockX:Number = numberValue(layout, "titleBlockX", 0.0);
            var titleBlockY:Number = numberValue(layout, "titleBlockY", 0.0);
            var categoryBlockX:Number = numberValue(layout, "categoryBlockX", 0.0);
            var categoryBlockY:Number = numberValue(layout, "categoryBlockY", 0.0);
            var headerBlockX:Number = numberValue(layout, "headerBlockX", 0.0);
            var headerBlockY:Number = numberValue(layout, "headerBlockY", 0.0);
            var footerBlockX:Number = numberValue(layout, "footerBlockX", 0.0);
            var footerBlockY:Number = numberValue(layout, "footerBlockY", 0.0);
            var micro:TextField = makeText("$AOZORA_TOP_MICRO", titleBlockX + 16, titleBlockY + 4,
                numberValue(layout, "topMicroFontSize", 8.0), themeColor(0.55), false);
            micro.alpha = numberValue(layout, "topMicroAlpha", 0.55);
            micro.width = width - 32.0; micro.height = 16.0;
            var microFormat:TextFormat = micro.defaultTextFormat;
            microFormat.letterSpacing = numberValue(layout, "topMicroLetterSpacing", 1.8);
            micro.defaultTextFormat = microFormat; micro.setTextFormat(microFormat);
            themeLayer.addChild(micro);

            var title:TextField = makeText("$AOZORA_TITLE",
                titleBlockX + numberValue(layout, "titleX", 16.0), titleBlockY + numberValue(layout, "titleY", 18.0),
                numberValue(layout, "titleFontSize", 29.0), themeColor(1.0), true);
            title.width = width - 32.0;
            title.height = 42.0;
            var titleFormat:TextFormat = title.defaultTextFormat;
            titleFormat.letterSpacing = numberValue(layout, "titleLetterSpacing", 0.6);
            title.defaultTextFormat = titleFormat; title.setTextFormat(titleFormat);
            themeLayer.addChild(title);

            var titleLine:Shape = new Shape();
            titleLine.graphics.lineStyle(2, themeColor(1.0), 0.90);
            titleLine.graphics.moveTo(titleBlockX + 16, titleBlockY + 55);
            titleLine.graphics.lineTo(titleBlockX + 150, titleBlockY + 55);
            themeLayer.addChild(titleLine);

            var categoryY:Number = categoryBlockY + numberValue(layout, "categoryY", 67.0);
            var categoryX:Number = categoryBlockX + numberValue(layout, "categoryX", 16.0);
            for (var i:int = 0; i < categoryLabels.length; ++i) {
                var tab:TextField = makeText(String(categoryLabels[i]),
                    categoryX + i * numberValue(layout, "categoryStep", 66.0), categoryY,
                    numberValue(layout, "categoryFontSize", 18.0), themeColor(1.0), true);
                tab.width = numberValue(layout, "categoryWidth", 54.0);
                tab.height = 28.0;
                tab.name = "categoryTab_" + i;
                tab.mouseEnabled = true;
                var tabFormat:TextFormat = tab.defaultTextFormat;
                tabFormat.align = "center";
                tabFormat.letterSpacing = numberValue(layout, "categoryLetterSpacing", 0.0);
                tab.defaultTextFormat = tabFormat;
                tab.setTextFormat(tabFormat);
                tab.mouseEnabled = false;
                themeLayer.addChild(tab);
                tabs.push(tab);

                var categoryHit:Sprite = new Sprite();
                categoryHit.name = "categoryHit_" + i;
                categoryHit.x = categoryX + i * numberValue(layout, "categoryStep", 66.0) +
                    numberValue(layout, "categoryHitX", 0.0);
                categoryHit.y = categoryY + numberValue(layout, "categoryHitY", -4.0);
                categoryHit.graphics.beginFill(0x000000, 0.01);
                categoryHit.graphics.drawRect(0, 0,
                    numberValue(layout, "categoryHitWidth", 66.0),
                    numberValue(layout, "categoryHitHeight", 34.0));
                categoryHit.graphics.endFill();
                categoryHit.mouseEnabled = true;
                categoryHit.mouseChildren = false;
                categoryHit.addEventListener(MouseEvent.MOUSE_OVER, onTabOver);
                categoryHit.addEventListener(MouseEvent.CLICK, onTabClick);
                themeLayer.addChild(categoryHit);
            }

            var categoryLine:Shape = new Shape();
            categoryLine.graphics.lineStyle(1, themeColor(1.0), numberValue(layout, "dividerAlpha", 0.36));
            categoryLine.graphics.moveTo(categoryBlockX + 16, categoryBlockY + 96);
            categoryLine.graphics.lineTo(categoryBlockX + width - 18, categoryBlockY + 96);
            themeLayer.addChild(categoryLine);

            var headerY:Number = numberValue(layout, "headerY", 108.0);
            addHeader("$AOZORA_HEADER_NAME", headerBlockX + numberValue(layout, "headerNameX", 47.0), headerBlockY + headerY, "left");
            addHeader("$AOZORA_HEADER_HOTKEY", headerBlockX + numberValue(layout, "headerHotkeyX", 210.0), headerBlockY + headerY, "center");
            addHeader("$AOZORA_HEADER_QUANTITY", headerBlockX + numberValue(layout, "headerQuantityX", 259.0), headerBlockY + headerY, "right");

            statusField = makeText("", headerBlockX + 16, headerBlockY + headerY + 18, 11, themeColor(0.70), false);
            statusField.width = width - 32; statusField.height = 22; statusField.visible = false;
            themeLayer.addChild(statusField);

            var footerX:Number = footerBlockX + numberValue(layout, "footerX", 16.0);
            var footerY:Number = footerBlockY + numberValue(layout, "footerY", 510.0);
            var footerLine:Shape = new Shape();
            footerLine.graphics.lineStyle(1, themeColor(1.0), numberValue(layout, "dividerAlpha", 0.36));
            footerLine.graphics.moveTo(footerX, footerY - 10);
            footerLine.graphics.lineTo(footerX + numberValue(layout, "footerWidth", 290.0), footerY - 10);
            themeLayer.addChild(footerLine);

            var gamepad:Boolean = focusSource == 2;
            addFooterHint(gamepad ? "A" : "E", "$AOZORA_ACTION_USE", footerX, footerY,
                numberValue(layout, "footerKey1X", 0.0), numberValue(layout, "footerKey1Y", 0.0),
                numberValue(layout, "footerKey1TextY", 1.0),
                numberValue(layout, "footerLabel1X", 36.0), numberValue(layout, "footerLabel1Y", 5.0), false);
            addFooterHint(gamepad ? "X" : "Q", "$AOZORA_ACTION_REMOVE", footerX, footerY,
                numberValue(layout, "footerKey2X", 151.0), numberValue(layout, "footerKey2Y", 0.0),
                numberValue(layout, "footerKey2TextY", 1.0),
                numberValue(layout, "footerLabel2X", 187.0), numberValue(layout, "footerLabel2Y", 5.0), true);
            var bottomLine:Shape = new Shape();
            bottomLine.graphics.lineStyle(1, themeColor(1.0), numberValue(layout, "dividerAlpha", 0.36));
            var footerHeight:Number = numberValue(layout, "footerHeight", 34.0);
            bottomLine.graphics.moveTo(footerX, footerY + footerHeight + 10.0);
            bottomLine.graphics.lineTo(footerX + numberValue(layout, "footerWidth", 290.0),
                footerY + footerHeight + 10.0);
            themeLayer.addChild(bottomLine);
            addBottomDetail(width, height, footerX, footerY);
            updateTabAppearance();
        }

        private function addBottomDetail(width:Number, height:Number, footerX:Number, footerY:Number):void
        {
            var detail:TextField = makeText("$AOZORA_FOOTER_DETAIL",
                footerX, footerY + numberValue(layout, "footerDetailY", 51.0),
                numberValue(layout, "footerDetailFontSize", 7.0), themeColor(0.55), false);
            detail.alpha = numberValue(layout, "footerDetailAlpha", 0.55);
            detail.width = width - 32.0; detail.height = 14.0;
            var detailFormat:TextFormat = detail.defaultTextFormat;
            detailFormat.letterSpacing = numberValue(layout, "footerDetailLetterSpacing", 1.0);
            detail.defaultTextFormat = detailFormat; detail.setTextFormat(detailFormat);
            themeLayer.addChild(detail);

            var mark:Shape = new Shape();
            mark.graphics.lineStyle(1, themeColor(0.58), 0.72);
            var cx:Number = footerX + numberValue(layout, "footerWidth", 290.0) - 28.0;
            var cy:Number = footerY + numberValue(layout, "footerDetailY", 51.0) + 5.0;
            mark.graphics.drawCircle(cx, cy, 5);
            mark.graphics.drawCircle(cx, cy, 2);
            mark.graphics.moveTo(cx - 12, cy); mark.graphics.lineTo(cx - 7, cy);
            mark.graphics.moveTo(cx + 7, cy); mark.graphics.lineTo(cx + 12, cy);
            themeLayer.addChild(mark);
        }

        private function addHeader(label:String, xPos:Number, yPos:Number, alignment:String):void
        {
            var field:TextField = makeText(label, xPos, yPos,
                numberValue(layout, "headerFontSize", 12.0), themeColor(0.68), false);
            if (alignment == "center") {
                field.x = xPos - 30.0;
                field.width = 60.0;
            } else if (alignment == "right") {
                field.width = numberValue(layout, "rowQuantityWidth", 39.0);
            } else {
                field.width = numberValue(layout, "rowNameWidth", 150.0);
            }
            field.height = 22;
            var format:TextFormat = field.defaultTextFormat;
            format.align = alignment;
            format.letterSpacing = numberValue(layout, "headerLetterSpacing", 0.0);
            field.defaultTextFormat = format; field.setTextFormat(format);
            field.mouseEnabled = false;
            themeLayer.addChild(field);
        }

        private function addFooterHint(key:String, label:String, footerX:Number, footerY:Number,
                keyX:Number, keyY:Number, keyTextY:Number, labelX:Number, labelY:Number, right:Boolean):void
        {
            var xPos:Number = footerX + keyX;
            var yPos:Number = footerY + keyY;
            var keyScale:Number = numberValue(layout, "footerKeyScale", 1.0);
            var keyWidth:Number = numberValue(layout, "footerKeyWidth", 28.0) * keyScale;
            var keyHeight:Number = numberValue(layout, "footerKeyHeight", 34.0) * keyScale;
            var keyBox:Shape = new Shape();
            keyBox.graphics.lineStyle(1.2, themeColor(1.0), 0.90);
            strokeKeyCap(keyBox.graphics, xPos, yPos, keyWidth, keyHeight, 3.0 * keyScale);
            themeLayer.addChild(keyBox);
            var keyField:TextField = makeText(key, xPos, yPos + keyTextY,
                numberValue(layout, "footerKeyFontSize", 16.0) * keyScale, themeColor(1.0), false);
            keyField.width = keyWidth; keyField.height = keyHeight - 2.0;
            var keyFormat:TextFormat = keyField.defaultTextFormat; keyFormat.align = "center";
            keyFormat.letterSpacing = numberValue(layout, "footerKeyLetterSpacing", 0.2);
            keyField.defaultTextFormat = keyFormat; keyField.setTextFormat(keyFormat);
            themeLayer.addChild(keyField);
            var labelField:TextField = makeText(label, footerX + labelX, footerY + labelY,
                numberValue(layout, "footerLabelFontSize", 15.0), themeColor(0.86), false);
            labelField.width = right ? 100 : 80;
            labelField.height = numberValue(layout, "footerHeight", 34.0);
            var labelFormat:TextFormat = labelField.defaultTextFormat;
            labelFormat.letterSpacing = numberValue(layout, "footerLabelLetterSpacing", 0.0);
            labelField.defaultTextFormat = labelFormat; labelField.setTextFormat(labelFormat);
            themeLayer.addChild(labelField);
            footerKeys.push(keyField); footerLabels.push(labelField);
            if (right) {
                var divider:Shape = new Shape();
                divider.graphics.lineStyle(1, themeColor(1.0), 0.45);
                var dividerX:Number = footerX + numberValue(layout, "footerDividerX", 129.0);
                var dividerY:Number = footerY + numberValue(layout, "footerDividerY", 1.0);
                var dividerHeight:Number = numberValue(layout, "footerDividerHeight", 28.0);
                divider.graphics.moveTo(dividerX, dividerY);
                divider.graphics.lineTo(dividerX, dividerY + dividerHeight);
                themeLayer.addChild(divider);
            }
        }

        private function rebuildRows():void
        {
            filterVisibleItems();
            if (selectedIndex >= visibleItems.length)
                selectedIndex = Math.max(0, visibleItems.length - 1);

            var rowHeight:Number = numberValue(layout, "rowHeight", 45.0);
            var listTop:Number = numberValue(layout, "listTop", 126.0);
            var footerY:Number = numberValue(layout, "footerY", 510.0);
            var maxRows:int = visibleRowCapacity();
            var maxStart:int = Math.max(0, visibleItems.length - maxRows);
            if (selectedIndex < scrollStart)
                scrollStart = selectedIndex;
            else if (selectedIndex >= scrollStart + maxRows)
                scrollStart = selectedIndex - maxRows + 1;
            scrollStart = Math.max(0, Math.min(scrollStart, maxStart));
            var start:int = scrollStart;
            var end:int = Math.min(visibleItems.length, start + maxRows);
            for (var i:int = start; i < end; ++i)
                createRow(visibleItems[i], i, i - start);
            updateScrollBar(maxRows, scrollStart);
            updateSelection();
            applyNameMarqueePosition();
            updateTabAppearance();
        }

        private function visibleRowCapacity():int
        {
            var rowHeight:Number = numberValue(layout, "rowHeight", 45.0);
            var listTop:Number = numberValue(layout, "listTop", 126.0);
            var footerY:Number = numberValue(layout, "footerY", 510.0);
            return Math.max(1, Math.min(10, int((footerY - listTop - 16.0) / rowHeight)));
        }

        private function filterVisibleItems():void
        {
            visibleItems = [];
            var category:String = String(categories[categoryIndex]);
            for each (var candidate:Object in items) {
                if (category == "ALL" ||
                    (category == "WEAP" && candidate.type == "WEAP") ||
                    (category == "ARMO" && candidate.type == "ARMO") ||
                    (category == "ALCH" && candidate.type == "ALCH"))
                    visibleItems.push(candidate);
            }
        }

        private function createRow(item:Object, index:int, rowIndex:int):void
        {
            var row:Sprite = new Sprite();
            row.name = "favoriteRow_" + index;
            row.x = numberValue(layout, "listBlockX", 0.0) + numberValue(layout, "listX", 12.0);
            row.y = numberValue(layout, "listBlockY", 0.0) + numberValue(layout, "listTop", 126.0) + rowIndex * numberValue(layout, "rowHeight", 45.0);
            row.mouseEnabled = true; row.mouseChildren = false; row.buttonMode = true;
            row.addEventListener(MouseEvent.MOUSE_OVER, onRowOver);
            row.addEventListener(MouseEvent.CLICK, onRowClick);

            var background:Shape = new Shape(); background.name = "rowBackground";
            var rowWidth:Number = numberValue(layout, "rowWidth", 296.0);
            var rowHeight:Number = numberValue(layout, "rowHeight", 45.0) - 3.0;
            fillRect(background.graphics, 0x010606, numberValue(layout, "rowAlpha", 0.22), 0, 0, rowWidth, rowHeight);
            background.graphics.lineStyle(1, themeColor(0.55), numberValue(layout, "dividerAlpha", 0.36));
            background.graphics.moveTo(0, rowHeight); background.graphics.lineTo(rowWidth, rowHeight);
            row.addChild(background);

            var rowIconX:Number = numberValue(layout, "rowIconX", 13.0);
            var rowIconY:Number = numberValue(layout, "rowIconY", 10.0);

            var rawName:String = item && item.name ? String(item.name) : "";
            var type:String = item && item.type ? String(item.type) : "OTHER";
            var iconLibrary:String = item && item.iconLibrary ? String(item.iconLibrary) : "";
            var iconClass:String = item && item.iconClass ? String(item.iconClass) : "";
            var iconCategory:String = item && item.fallbackIconType ? String(item.fallbackIconType) :
                (item && item.iconCategory ? String(item.iconCategory) : "");
            var icon:Sprite = makeItemIcon(type, rawName, iconLibrary, iconClass, iconCategory);
            icon.name = "itemIcon";
            icon.x = rowIconX;
            icon.y = rowIconY;
            icon.scaleX = numberValue(layout, "rowIconScale", 1.0);
            icon.scaleY = icon.scaleX;
            row.addChild(icon);

            var rowTextBlockY:Number = numberValue(layout, "rowTextBlockY", 0.0);
            var nameWidth:Number = numberValue(layout, "rowNameWidth", 150.0);
            var nameField:TextField = makeText(cleanName(rawName),
                numberValue(layout, "rowNameX", 47.0), numberValue(layout, "rowNameY", 7.0) + rowTextBlockY,
                numberValue(layout, "rowFontSize", 17.0), themeColor(0.92), false);
            nameField.name = "itemName";
            nameField.width = nameWidth;
            nameField.height = rowHeight - 4.0;
            var nameFormat:TextFormat = nameField.defaultTextFormat;
            nameFormat.letterSpacing = numberValue(layout, "rowLetterSpacing", 0.0);
            nameField.defaultTextFormat = nameFormat; nameField.setTextFormat(nameFormat);
            row.addChild(nameField);

            var hotkeyScale:Number = numberValue(layout, "rowHotkeyScale", 1.0);
            var hotkeyWidth:Number = numberValue(layout, "rowHotkeyWidth", 30.0) * hotkeyScale;
            var hotkeyHeight:Number = numberValue(layout, "rowHotkeyHeight", 29.0) * hotkeyScale;
            var hotkeyX:Number = numberValue(layout, "rowHotkeyX", 210.0);
            var hotkeyY:Number = numberValue(layout, "rowHotkeyY", 7.0);
            var hotkeyBox:Shape = new Shape(); hotkeyBox.name = "hotkeyBox";
            hotkeyBox.graphics.lineStyle(1.2, themeColor(0.92), 0.90);
            strokeKeyCap(hotkeyBox.graphics, hotkeyX, hotkeyY, hotkeyWidth, hotkeyHeight, 3.0 * hotkeyScale);
            row.addChild(hotkeyBox);
            var hotkey:int = item && item.hotkey is Number ? int(item.hotkey) : -1;
            var hotkeyField:TextField = makeText(hotkeyLabel(hotkey),
                hotkeyX, numberValue(layout, "rowHotkeyTextY", 7.0) + rowTextBlockY,
                numberValue(layout, "rowHotkeyFontSize", 16.0) * hotkeyScale,
                hotkey >= 0 ? themeColor(0.92) : themeColor(0.50), false);
            hotkeyField.name = "hotkeyField"; hotkeyField.width = hotkeyWidth; hotkeyField.height = hotkeyHeight - 2.0;
            var hotkeyFormat:TextFormat = hotkeyField.defaultTextFormat; hotkeyFormat.align = "center";
            hotkeyFormat.letterSpacing = numberValue(layout, "rowHotkeyLetterSpacing", 0.2);
            hotkeyField.defaultTextFormat = hotkeyFormat; hotkeyField.setTextFormat(hotkeyFormat); row.addChild(hotkeyField);

            var count:int = item && item.count is Number ? int(item.count) : 0;
            var displayCount:int = Math.max(0, Math.min(999, count));
            // The quantity prefix is a universal HUD unit. Keeping it literal
            // avoids creating a composite "$KEY" value that the Scaleform
            // translator cannot resolve at runtime.
            var quantityLabel:String = count > 999 ? "x999+" : "x" + displayCount;
            var quantity:TextField = makeText(quantityLabel,
                numberValue(layout, "rowQuantityX", 257.0), numberValue(layout, "rowQuantityY", 8.0) + rowTextBlockY,
                numberValue(layout, "rowQuantityFontSize", 15.5), themeColor(0.92), false);
            quantity.name = "quantityField"; quantity.width = numberValue(layout, "rowQuantityWidth", 39.0); quantity.height = 27;
            var quantityFormat:TextFormat = quantity.defaultTextFormat; quantityFormat.align = "center";
            quantityFormat.letterSpacing = numberValue(layout, "rowQuantityLetterSpacing", 0.2);
            quantity.defaultTextFormat = quantityFormat; quantity.setTextFormat(quantityFormat); row.addChild(quantity);

            rowLayer.addChild(row); rows.push(row);
        }

        private function updateSelection():void
        {
            var focusItem:Object = selectedIndex >= 0 && selectedIndex < visibleItems.length ? visibleItems[selectedIndex] : null;
            var focusKey:String = focusItem ?
                String(focusItem.formID is Number ? focusItem.formID : "") + ":" +
                String(focusItem.instanceKey ? focusItem.instanceKey : "") : "";
            if (focusKey != nameMarqueeSignature) {
                nameMarqueeSignature = focusKey;
                resetNameMarquee();
            }
            for each (var row:Sprite in rows) {
                var marker:int = row.name.indexOf("_");
                var index:int = marker >= 0 ? int(row.name.substr(marker + 1)) : -1;
                var selected:Boolean = index == selectedIndex;
                var hovered:Boolean = index == hoverIndex;
                var active:Boolean = selected || hovered;
                var rowItem:Object = index >= 0 && index < visibleItems.length ? visibleItems[index] : null;
                var equipped:Boolean = rowItem && Boolean(rowItem.equipped);
                var background:Shape = row.getChildByName("rowBackground") as Shape;
                var rowWidth:Number = numberValue(layout, "rowWidth", 296.0);
                var rowHeight:Number = numberValue(layout, "rowHeight", 45.0) - 3.0;
                var focusFrameX:Number = numberValue(layout, "focusFrameX", 0.0);
                var focusFrameY:Number = numberValue(layout, "focusFrameY", 0.0);
                var focusFrameWidth:Number = numberValue(layout, "focusFrameWidth", rowWidth);
                var focusFrameHeight:Number = numberValue(layout, "focusFrameHeight", rowHeight);
                var focusFrameThickness:Number = numberValue(layout, "focusFrameThickness", 1.0);
                var focusFrameAlpha:Number = numberValue(layout, "focusFrameAlpha", 0.98);
                background.graphics.clear();
                fillRect(background.graphics, selected ? 0x3B2A0B : (equipped ? equippedColor : 0x010606),
                    selected ? 0.34 : (equipped ? numberValue(layout, "equippedRowAlpha", 0.10) :
                        numberValue(layout, "rowAlpha", 0.22)), 0, 0, rowWidth, rowHeight);
                background.graphics.lineStyle(active ? focusFrameThickness : 1.0,
                    active ? focusColor : (equipped ? equippedColor : themeColor(0.55)),
                    active ? (selected ? focusFrameAlpha : focusFrameAlpha * 0.62) :
                        numberValue(layout, "dividerAlpha", 0.36));
                if (selected)
                    strokeRect(background.graphics, focusFrameX, focusFrameY, focusFrameWidth, focusFrameHeight);
                else if (hovered)
                    strokeRect(background.graphics, focusFrameX, focusFrameY, focusFrameWidth, focusFrameHeight);
                else {
                    background.graphics.moveTo(0, rowHeight); background.graphics.lineTo(rowWidth, rowHeight);
                }

                // Focus is represented by the amber frame only. Keep the
                // equipped cyan state visible while the row is selected.
                setTextColor(nameFieldForRow(row), equipped ? equippedColor : themeColor(0.92));
                var selectedItem:Object = index >= 0 && index < visibleItems.length ? visibleItems[index] : null;
                var hasHotkey:Boolean = selectedItem && selectedItem.hotkey is Number && int(selectedItem.hotkey) >= 0;
                setTextColor(row.getChildByName("hotkeyField") as TextField,
                    equipped ? equippedColor :
                        (hasHotkey ? themeColor(0.92) : themeColor(0.50)));
                setTextColor(row.getChildByName("quantityField") as TextField,
                    equipped ? equippedColor : themeColor(0.92));
                setIconColor(row.getChildByName("itemIcon") as Sprite,
                    equipped ? equippedColor : themeColor(1.0));
                var hotkeyBox:Shape = row.getChildByName("hotkeyBox") as Shape;
                var hotkeyScale:Number = numberValue(layout, "rowHotkeyScale", 1.0);
                var hotkeyWidth:Number = numberValue(layout, "rowHotkeyWidth", 30.0) * hotkeyScale;
                var hotkeyHeight:Number = numberValue(layout, "rowHotkeyHeight", 29.0) * hotkeyScale;
                hotkeyBox.graphics.clear();
                hotkeyBox.graphics.lineStyle(1.2, equipped ? equippedColor : themeColor(0.92), 0.90);
                strokeKeyCap(hotkeyBox.graphics, numberValue(layout, "rowHotkeyX", 210.0),
                    numberValue(layout, "rowHotkeyY", 7.0), hotkeyWidth, hotkeyHeight, 3.0 * hotkeyScale);
            }
            updateActionLabel();
        }

        private function updateActionLabel():void
        {
            if (!footerLabels || footerLabels.length == 0)
                return;
            var labelField:TextField = footerLabels[0] as TextField;
            if (!labelField)
                return;
            var item:Object = selectedIndex >= 0 && selectedIndex < visibleItems.length ?
                visibleItems[selectedIndex] : null;
            var label:String = "$AOZORA_ACTION_USE";
            if (item) {
                var type:String = item.type ? String(item.type) : "";
                if (type == "WEAP" || type == "ARMO")
                    label = Boolean(item.equipped) ? "$AOZORA_ACTION_UNEQUIP" : "$AOZORA_ACTION_EQUIP";
            }
            labelField.text = label;
        }

        private function nameFieldForRow(row:Sprite):TextField
        {
            return row ? row.getChildByName("itemName") as TextField : null;
        }

        private function resetNameMarquee():void
        {
            nameMarqueePhase = 0;
            nameMarqueeTimer = 0.0;
            nameMarqueeOffset = 0.0;
            var row:Sprite = rowLayer ? rowLayer.getChildByName("favoriteRow_" + selectedIndex) as Sprite : null;
            var field:TextField = nameFieldForRow(row);
            if (field)
                field.scrollH = 0;
        }

        private function updateNameMarquee(delta:Number):void
        {
            var row:Sprite = rowLayer ? rowLayer.getChildByName("favoriteRow_" + selectedIndex) as Sprite : null;
            var field:TextField = nameFieldForRow(row);
            if (!field || field.textWidth <= field.width + 1.0) {
                if (field)
                    field.scrollH = 0;
                return;
            }

            var delay:Number = numberValue(layout, "nameScrollDelay", 0.80);
            var speed:Number = numberValue(layout, "nameScrollSpeed", 24.0);
            var endPause:Number = numberValue(layout, "nameScrollEndPause", 0.90);
            var maxOffset:Number = Math.max(0.0, field.maxScrollH);
            if (maxOffset <= 0.0)
                return;

            if (nameMarqueePhase == 0) {
                nameMarqueeTimer += delta;
                if (nameMarqueeTimer >= delay) {
                    nameMarqueePhase = 1;
                    nameMarqueeTimer = 0.0;
                }
            } else if (nameMarqueePhase == 1) {
                nameMarqueeOffset = Math.min(maxOffset, nameMarqueeOffset + speed * delta);
                field.scrollH = int(Math.round(nameMarqueeOffset));
                if (nameMarqueeOffset >= maxOffset) {
                    nameMarqueePhase = 2;
                    nameMarqueeTimer = 0.0;
                }
            } else {
                field.scrollH = int(maxOffset);
                nameMarqueeTimer += delta;
                if (nameMarqueeTimer >= endPause) {
                    nameMarqueePhase = 0;
                    nameMarqueeTimer = 0.0;
                    nameMarqueeOffset = 0.0;
                    field.scrollH = 0;
                }
            }
        }

        private function applyNameMarqueePosition():void
        {
            var row:Sprite = rowLayer ? rowLayer.getChildByName("favoriteRow_" + selectedIndex) as Sprite : null;
            var field:TextField = nameFieldForRow(row);
            if (!field)
                return;
            if (nameMarqueePhase == 1)
                field.scrollH = int(Math.round(nameMarqueeOffset));
            else if (nameMarqueePhase == 2)
                field.scrollH = int(field.maxScrollH);
            else
                field.scrollH = 0;
        }

        private function updateTabAppearance():void
        {
            var xPos:Number = numberValue(layout, "categoryBlockX", 0.0) +
                numberValue(layout, "categoryX", 16.0) + categoryIndex * numberValue(layout, "categoryStep", 66.0);
            var underline:Shape = themeLayer.getChildByName("activeCategoryUnderline") as Shape;
            if (!underline) {
                underline = new Shape(); underline.name = "activeCategoryUnderline"; themeLayer.addChild(underline);
            }
            underline.graphics.clear();
            underline.graphics.lineStyle(2, focusColor, 0.98);
            var yPos:Number = numberValue(layout, "categoryBlockY", 0.0) + numberValue(layout, "categoryY", 67.0) + 27;
            underline.graphics.moveTo(xPos + 4, yPos);
            underline.graphics.lineTo(xPos + numberValue(layout, "categoryWidth", 54.0) - 4, yPos);
            for (var i:int = 0; i < tabs.length; ++i)
                setTextColor(tabs[i] as TextField,
                    (i == categoryIndex || i == hoverCategoryIndex) ? focusColor : themeColor(1.0));

            var hoverLine:Shape = themeLayer.getChildByName("hoverCategoryIndicator") as Shape;
            if (!hoverLine) {
                hoverLine = new Shape();
                hoverLine.name = "hoverCategoryIndicator";
                themeLayer.addChild(hoverLine);
            }
            hoverLine.graphics.clear();
            if (hoverCategoryIndex >= 0 && hoverCategoryIndex != categoryIndex) {
                var hoverX:Number = numberValue(layout, "categoryBlockX", 0.0) +
                    numberValue(layout, "categoryX", 16.0) + hoverCategoryIndex * numberValue(layout, "categoryStep", 66.0);
                hoverLine.graphics.lineStyle(2, focusColor, 0.72);
                hoverLine.graphics.moveTo(hoverX + 8, yPos);
                hoverLine.graphics.lineTo(hoverX + numberValue(layout, "categoryWidth", 54.0) - 8, yPos);
            }
        }

        private function updateScrollBar(maxRows:int, currentStart:int):void
        {
            var old:DisplayObject = themeLayer.getChildByName("scrollbar");
            if (old)
                themeLayer.removeChild(old);
            var rail:Sprite = new Sprite(); rail.name = "scrollbar";
            var g:Graphics = rail.graphics;
            var xPos:Number = numberValue(layout, "listBlockX", 0.0) + numberValue(layout, "scrollbarX", 307.0);
            var yPos:Number = numberValue(layout, "listBlockY", 0.0) + numberValue(layout, "scrollbarY", 128.0);
            var railHeight:Number = numberValue(layout, "scrollbarHeight", 360.0);
            var railWidth:Number = numberValue(layout, "scrollbarWidth", 4.0);
            g.lineStyle(1, themeColor(0.60), 0.46);
            g.moveTo(xPos + railWidth / 2, yPos + 14); g.lineTo(xPos + railWidth / 2, yPos + railHeight - 14);
            drawTriangle(g, xPos + railWidth / 2, yPos + 4, 5, true, themeColor(0.80));
            drawTriangle(g, xPos + railWidth / 2, yPos + railHeight - 4, 5, false, themeColor(0.80));
            var total:int = visibleItems.length;
            var thumbHeight:Number = total <= maxRows ? railHeight - 28 : Math.max(numberValue(layout, "scrollbarThumbMinHeight", 26.0), (railHeight - 28) * maxRows / total);
            var maxStart:int = Math.max(1, total - maxRows);
            var start:int = Math.max(0, Math.min(currentStart, maxStart));
            var thumbY:Number = yPos + 14 + (railHeight - 28 - thumbHeight) * (maxStart == 0 ? 0 : start / maxStart);
            fillRect(g, themeColor(1.0), 0.82, xPos, thumbY, railWidth, thumbHeight);
            themeLayer.addChild(rail);
        }

        private function drawTriangle(g:Graphics, xPos:Number, yPos:Number, size:Number, up:Boolean, color:uint):void
        {
            g.beginFill(color, 0.82);
            if (up) {
                g.moveTo(xPos, yPos - size); g.lineTo(xPos - size, yPos + size); g.lineTo(xPos + size, yPos + size);
            } else {
                g.moveTo(xPos, yPos + size); g.lineTo(xPos - size, yPos - size); g.lineTo(xPos + size, yPos - size);
            }
            g.lineTo(xPos, up ? yPos - size : yPos + size); g.endFill();
        }

        private function updateMascotLayout():void
        {
            if (!mascotController)
                return;
            var mascot:Object = null;
            if (layout.mascots is Array) {
                for each (var candidate:Object in layout.mascots as Array) {
                    if (candidate && candidate.group == mascotController.groupName) {
                        mascot = candidate; break;
                    }
                }
            }
            mascotController.x = numberValue(mascot, "x", 230.0);
            mascotController.y = numberValue(mascot, "y", -42.0);
            mascotController.scaleX = numberValue(mascot, "scale", 0.255);
            mascotController.scaleY = mascotController.scaleX;
            mascotController.rotation = numberValue(mascot, "rotation", 0.0);
            mascotController.alpha = mascotEnabled ? numberValue(mascot, "alpha", 0.96) : 0.0;
            mascotController.visible = mascotEnabled;
            // Keep the independent Vault-Tec backdrop visible when the
            // mascot itself is disabled in MCM.
            mascotLayer.visible = true;
        }

        private function mountExternalAssets():void
        {
            if (!f4seCodeObject || silhouetteMounted)
                return;
            var silhouetteFiles:Array = [
                ["skyui_like_silhouette_vault_boy.dds", "AozoraFavoritesSkyuiLikeVaultBoy"],
                ["skyui_like_silhouette_vault_boy_pipboy.dds", "AozoraFavoritesSkyuiLikeVaultBoyPipboy"]
            ];
            var silhouetteIndex:int = int(Math.floor(Math.random() * silhouetteFiles.length));
            if (silhouetteFiles.length > 1 && silhouetteIndex == lastSilhouetteIndex)
                silhouetteIndex = (silhouetteIndex + 1) % silhouetteFiles.length;
            lastSilhouetteIndex = silhouetteIndex;
            var choice:Array = silhouetteFiles[silhouetteIndex];
            silhouetteId = String(choice[1]);
            var mounted:Boolean = Boolean(f4seCodeObject.MountImage(
                "AozoraFavoritesMenu", "Interface\\AozoraFavorites\\" + choice[0], silhouetteId));
            silhouetteMounted = true;
            if (mounted) {
                var loader:Loader = new Loader();
                loader.contentLoaderInfo.addEventListener(Event.COMPLETE, onSilhouetteLoaded);
                loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, onSilhouetteError);
                loader.load(new URLRequest("img://" + silhouetteId));
            }
            var backdropId:String = "AozoraFavoritesSkyuiLikeVaultTecBackdrop";
            var backdropMounted:Boolean = Boolean(f4seCodeObject.MountImage(
                "AozoraFavoritesMenu", "Interface\\AozoraFavorites\\skyui_like_silhouette_vault_tec.dds", backdropId));
            if (backdropMounted) {
                var backdropLoader:Loader = new Loader();
                backdropLoader.contentLoaderInfo.addEventListener(Event.COMPLETE, onMascotBackdropLoaded);
                backdropLoader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, onMascotBackdropError);
                backdropLoader.load(new URLRequest("img://" + backdropId));
            }
            mascotController.attachImageBridge(f4seCodeObject);
        }

        private function onSilhouetteLoaded(event:Event):void
        {
            var info:Object = event.currentTarget;
            var loader:Loader = info && info.loader is Loader ? info.loader as Loader : null;
            var bitmap:Bitmap = loader ? loader.content as Bitmap : null;
            if (!bitmap || !bitmap.bitmapData)
                return;
            silhouetteBitmap = bitmap;
            rebuildVisual();
        }

        private function onSilhouetteError(event:IOErrorEvent):void
        {
            silhouetteBitmap = null;
        }

        private function onMascotBackdropLoaded(event:Event):void
        {
            var info:Object = event.currentTarget;
            var loader:Loader = info && info.loader is Loader ? info.loader as Loader : null;
            var bitmap:Bitmap = loader ? loader.content as Bitmap : null;
            if (!bitmap || !bitmap.bitmapData)
                return;
            mascotBackdrop.bitmapData = bitmap.bitmapData;
            mascotBackdrop.smoothing = true;
            updateMascotBackdrop();
        }

        private function onMascotBackdropError(event:IOErrorEvent):void
        {
            if (mascotBackdrop)
                mascotBackdrop.visible = false;
        }

        private function onMascotReady(controller:SkyuiLikeLookController):void
        {
            updateMascotLayout();
        }

        private function onMascotError(message:String):void
        {
            mascotController.visible = false;
        }

        private function findF4SECodeObject():void
        {
            var rootObject:Object = root as Object;
            var f4se:Object = rootObject ? rootObject["f4se"] : null;
            if (f4se && f4se.MountImage is Function) {
                OnF4SEObjCreated(f4se);
                return;
            }
            addEventListener(Event.ENTER_FRAME, retryF4SECodeObject);
        }

        private function retryF4SECodeObject(event:Event):void
        {
            removeEventListener(Event.ENTER_FRAME, retryF4SECodeObject);
            findF4SECodeObject();
        }

        private function writeDiagnostic(message:String):void
        {
            if (BGSCodeObj && BGSCodeObj.WriteLog is Function)
                BGSCodeObj.WriteLog(message);
        }

        private function diagnoseEquipmentSnapshot():void
        {
            var count:int = 0;
            var indices:String = "";
            for (var i:int = 0; i < items.length; ++i) {
                var item:Object = items[i];
                if (item && Boolean(item.equipped)) {
                    ++count;
                    indices += (indices.length > 0 ? "," : "") + i;
                }
            }
            var signature:String = count + ":" + indices;
            if (signature != lastEquipmentDiagnostic) {
                lastEquipmentDiagnostic = signature;
                writeDiagnostic("SWF_EQUIPPED count=" + count + " indices=" + indices);
                if (count > 0)
                    writeDiagnostic("EQUIPPED_ROW_DRAW requested indices=" + indices);
            }
        }

        private function loadIconLibrary():void
        {
            requestIconLibrary("FallUI_IconLib.swf");
        }

        private function normalizeIconLibraryPath(path:String):String
        {
            var normalized:String = path ? String(path) : "";
            normalized = normalized.split("\\").join("/");
            while (normalized.indexOf("./") == 0)
                normalized = normalized.substr(2);
            if (normalized.indexOf("Interface/") == 0)
                normalized = normalized.substr(10);
            return normalized;
        }

        private function requestIconLibrary(path:String):void
        {
            var key:String = normalizeIconLibraryPath(path);
            if (!key.length || iconLibraryLoaders[key])
                return;
            var loader:Loader = new Loader();
            loader.name = "aozoraIconLibrary:" + key;
            loader.contentLoaderInfo.addEventListener(Event.COMPLETE, onIconLibraryLoaded);
            loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, onIconLibraryError);
            iconLibraryLoaders[key] = loader;
            if (key == "FallUI_IconLib.swf")
                iconLibraryLoader = loader;
            loader.load(new URLRequest(key));
        }

        private function onIconLibraryLoaded(event:Event):void
        {
            var info:LoaderInfo = event.currentTarget as LoaderInfo;
            var loader:Loader = info ? info.loader : null;
            var key:String = loader && loader.name ? String(loader.name).replace(/^aozoraIconLibrary:/, "") : "";
            var domain:ApplicationDomain = info ? info.applicationDomain : null;
            if (key.length)
                iconLibraryDomains[key] = domain;
            if (iconLibraryRetryTimers[key] != null) {
                clearTimeout(uint(iconLibraryRetryTimers[key]));
                delete iconLibraryRetryTimers[key];
            }
            delete iconLibraryRetries[key];
            if (key == "FallUI_IconLib.swf") {
                iconLibraryDomain = domain;
                iconLibraryReady = domain != null;
                writeDiagnostic("ICON_LIBRARY loaded=" + (iconLibraryReady ? "1" : "0") +
                    " pistol=" + (iconLibraryReady && domain.hasDefinition("m_M8r.Fo4Wpn.Pistol") ? "1" : "0"));
            } else {
                writeDiagnostic("ICON_LIBRARY loaded=" + (domain ? "1" : "0") + " path=" + key);
            }
            if (domain && items.length > 0)
                rebuildVisual();
        }

        private function onIconLibraryError(event:IOErrorEvent):void
        {
            var info:LoaderInfo = event.currentTarget as LoaderInfo;
            var loader:Loader = info ? info.loader : null;
            var key:String = loader && loader.name ? String(loader.name).replace(/^aozoraIconLibrary:/, "") : "";
            var retried:Boolean = Boolean(iconLibraryRetries[key]);
            writeDiagnostic("ICON_LIBRARY load-error path=" + key + " retry=" + (retried ? "0" : "1"));
            if (retried) {
                // A failed Loader must not permanently occupy the request
                // cache. The next visual request can create a fresh Loader.
                if (iconLibraryRetryTimers[key] != null) {
                    clearTimeout(uint(iconLibraryRetryTimers[key]));
                    delete iconLibraryRetryTimers[key];
                }
                delete iconLibraryLoaders[key];
                delete iconLibraryRetries[key];
                delete iconLibraryDomains[key];
                if (key == "FallUI_IconLib.swf") {
                    iconLibraryLoader = null;
                    iconLibraryDomain = null;
                    iconLibraryReady = false;
                }
                writeDiagnostic("ICON_LIBRARY unavailable after retry path=" + key);
                return;
            }
            iconLibraryRetries[key] = true;
            if (iconLibraryRetryTimers[key] == null)
                iconLibraryRetryTimers[key] = setTimeout(retryIconLibrary, 200, key);
            writeDiagnostic("ICON_LIBRARY retry-scheduled path=" + key + " delayMs=200");
        }

        private function retryIconLibrary(key:String):void
        {
            delete iconLibraryRetryTimers[key];
            var loader:Loader = iconLibraryLoaders[key] as Loader;
            if (!loader) {
                delete iconLibraryRetries[key];
                requestIconLibrary(key);
                return;
            }
            var retryPath:String = key == "FallUI_IconLib.swf" ?
                "Interface/FallUI_IconLib.swf" : key;
            loader.load(new URLRequest(retryPath));
        }

        private function animateVisuals(event:Event):void
        {
            if (scanlineLayer)
                scanlineLayer.y = (scanlineLayer.y + 0.12) % 4.0;
            updateNameMarquee(1.0 / 30.0);
            if (layout && Boolean(layout.editorMode) && BGSCodeObj && BGSCodeObj.RequestSnapshot is Function) {
                editorElapsed += 1.0 / 30.0;
                var refresh:Number = numberValue(layout, "refreshSeconds", 0.50);
                if (refresh > 0 && editorElapsed >= refresh) {
                    editorElapsed = 0.0;
                    BGSCodeObj.RequestSnapshot();
                }
            }
        }

        private function onTabOver(event:MouseEvent):void
        {
            mouseMovedSinceInput = true;
            hoverCategoryIndex = categoryIndexFromTarget(event.currentTarget);
            updateTabAppearance();
        }

        private function onTabClick(event:MouseEvent):void
        {
            mouseMovedSinceInput = true;
            hoverCategoryIndex = -1;
            selectCategoryFromMouse(categoryIndexFromTarget(event.currentTarget));
        }

        private function onRowOver(event:MouseEvent):void
        {
            if (focusSource != 3 && !mouseMovedSinceInput)
                return;
            mouseMovedSinceInput = true;
            selectRowFromMouse(event.currentTarget as Sprite);
        }

        private function onRowClick(event:MouseEvent):void
        {
            var row:Sprite = event.currentTarget as Sprite;
            if (!row)
                return;
            mouseMovedSinceInput = true;
            hoverIndex = -1;
            selectRowFromMouse(row);
            ActivateSelected();
        }

        private function onMouseMove(event:MouseEvent):void
        {
            var moved:Boolean = !hasMousePosition ||
                Math.abs(event.stageX - lastMouseStageX) > 0.1 ||
                Math.abs(event.stageY - lastMouseStageY) > 0.1;
            if (!moved)
                return;
            hasMousePosition = true;
            lastMouseStageX = event.stageX;
            lastMouseStageY = event.stageY;
            mouseMovedSinceInput = true;
            var row:Sprite = event.target as Sprite;
            if (row && row.name && row.name.indexOf("favoriteRow_") == 0) {
                if (hoverCategoryIndex >= 0) {
                    hoverCategoryIndex = -1;
                    updateTabAppearance();
                }
                selectRowFromMouse(row);
            } else {
                var tabTarget:Object = event.target;
                if (tabTarget && tabTarget.name &&
                    (String(tabTarget.name).indexOf("categoryHit_") == 0 ||
                        String(tabTarget.name).indexOf("categoryTab_") == 0)) {
                    hoverCategoryIndex = categoryIndexFromTarget(tabTarget);
                    updateTabAppearance();
                    return;
                }
                if (hoverIndex >= 0) {
                    hoverIndex = -1;
                    updateSelection();
                }
                if (hoverCategoryIndex >= 0) {
                    hoverCategoryIndex = -1;
                    updateTabAppearance();
                }
            }
        }

        private function onMouseOut(event:MouseEvent):void
        {
            if (event.relatedObject != null)
                return;
            hoverIndex = -1;
            hoverCategoryIndex = -1;
            updateSelection();
            updateTabAppearance();
        }

        private function onMouseWheel(event:MouseEvent):void
        {
            if (!event || !rowLayer || visibleItems.length == 0)
                return;

            var local:Point = rowLayer.globalToLocal(new Point(event.stageX, event.stageY));
            var rowWidth:Number = numberValue(layout, "rowWidth", 296.0);
            var rowHeight:Number = numberValue(layout, "rowHeight", 45.0);
            var listLeft:Number = numberValue(layout, "listBlockX", 0.0) +
                numberValue(layout, "listX", 12.0);
            var listTop:Number = numberValue(layout, "listBlockY", 0.0) +
                numberValue(layout, "listTop", 126.0);
            var maxRows:int = visibleRowCapacity();
            var listBottom:Number = listTop + maxRows * rowHeight;
            var scrollbarLeft:Number = numberValue(layout, "listBlockX", 0.0) +
                numberValue(layout, "scrollbarX", 307.0) - 8.0;
            var scrollbarRight:Number = scrollbarLeft +
                Math.max(16.0, numberValue(layout, "scrollbarWidth", 4.0) + 8.0);
            var inRows:Boolean = local.x >= listLeft && local.x <= listLeft + rowWidth &&
                local.y >= listTop && local.y <= listBottom;
            var inScrollbar:Boolean = local.x >= scrollbarLeft && local.x <= scrollbarRight &&
                local.y >= listTop && local.y <= listBottom;
            if (!inRows && !inScrollbar)
                return;

            var maxStart:int = Math.max(0, visibleItems.length - maxRows);
            if (maxStart > 0 && event.delta != 0) {
                var direction:int = event.delta > 0 ? -1 : 1;
                scrollStart = Math.max(0, Math.min(maxStart, scrollStart + direction));
                if (selectedIndex < scrollStart)
                    selectedIndex = scrollStart;
                else if (selectedIndex >= scrollStart + maxRows)
                    selectedIndex = scrollStart + maxRows - 1;
                hoverIndex = -1;
                rebuildVisual();
            }
            event.preventDefault();
            event.stopImmediatePropagation();
        }

        private function categoryIndexFromTarget(target:Object):int
        {
            if (!target || !target.name)
                return -1;
            var marker:int = String(target.name).indexOf("_");
            if (marker < 0)
                return -1;
            return clampInt(int(String(target.name).substr(marker + 1)), -1, categories.length - 1);
        }

        private function selectCategoryFromMouse(nextCategory:int):void
        {
            if (nextCategory < 0 || nextCategory >= categories.length)
                return;
            categoryIndex = nextCategory; selectedIndex = 0; scrollStart = 0; hoverIndex = -1; focusSource = 3;
            rebuildRows(); updateTabAppearance();
            if (BGSCodeObj && BGSCodeObj.SelectCategory is Function)
                BGSCodeObj.SelectCategory(nextCategory);
        }

        private function selectRowFromMouse(row:Sprite):void
        {
            if (!row || !row.name)
                return;
            var nextIndex:int = int(row.name.substr(row.name.indexOf("_") + 1));
            if (nextIndex < 0 || nextIndex >= visibleItems.length)
                return;
            var changed:Boolean = nextIndex != selectedIndex;
            hoverIndex = nextIndex;
            selectedIndex = nextIndex; focusSource = 3;
            if (changed && mascotController)
                mascotController.onFocusChanged();
            updateSelection();
            if ((changed || focusSource != 3) && BGSCodeObj && BGSCodeObj.SelectFavorite is Function) {
                var item:Object = visibleItems[selectedIndex];
                BGSCodeObj.SelectFavorite(item.formID, item.instanceKey);
            }
        }

        private function setHoverRow(row:Sprite):void
        {
            if (!row || !row.name)
                return;
            var marker:int = row.name.indexOf("_");
            if (marker < 0)
                return;
            var nextIndex:int = int(row.name.substr(marker + 1));
            if (nextIndex < 0 || nextIndex >= visibleItems.length)
                return;
            if (hoverIndex != nextIndex) {
                hoverIndex = nextIndex;
                updateSelection();
            }
        }

        private function ActivateSelected():void
        {
            if (selectedIndex < 0 || selectedIndex >= visibleItems.length)
                return;
            var item:Object = visibleItems[selectedIndex];
            if (BGSCodeObj && BGSCodeObj.ActivateFavorite is Function)
                BGSCodeObj.ActivateFavorite(item.formID, item.instanceKey);
        }

        private function makeItemIcon(type:String, rawName:String, iconLibrary:String = "",
            iconClass:String = "", iconCategory:String = ""):Sprite
        {
            if (iconMode == 1)
                return FallbackIconFactory.create(type, iconCategory);
            var fis:Sprite = makeFallUIIcon(rawName, type, iconLibrary, iconClass);
            if (fis)
                return fis;
            // Aozora Basic Icon is the guaranteed local fallback. FIS is
            // preferred when it resolves, but never required for a usable UI.
            return FallbackIconFactory.create(type, iconCategory);
        }

        private function makeFallUIIcon(rawName:String, type:String, nativeIconLibrary:String = "", nativeIconClass:String = ""):Sprite
        {
            var libraryKey:String = normalizeIconLibraryPath(nativeIconLibrary.length ? nativeIconLibrary : "FallUI_IconLib.swf");
            var domain:ApplicationDomain = iconLibraryDomains[libraryKey] as ApplicationDomain;
            if (!domain) {
                requestIconLibrary(libraryKey);
                if (!iconDiagnosticKeys["library-pending"]) {
                    iconDiagnosticKeys["library-pending"] = true;
                    writeDiagnostic("ICON_LIBRARY pending path=" + libraryKey);
                }
                return null;
            }
            // FIS is resolved by Native from the active tag configuration.
            // Keep the AS3 side to the community-tested m_ + ApplicationDomain lookup;
            // never guess an icon from the item's display name here.
            var className:String = nativeIconClass && nativeIconClass.length ? nativeIconClass : "";
            var definitionName:String = className ? "m_" + className : "";
            if (!className || !domain.hasDefinition(definitionName)) {
                var missingKey:String = "missing:" + className;
                if (!iconDiagnosticKeys[missingKey]) {
                    iconDiagnosticKeys[missingKey] = true;
                    writeDiagnostic("ICON_CLASS missing=" + className + " type=" + type + " library=" + libraryKey);
                }
                return null;
            }
            try {
                var displayClass:Class = domain.getDefinition(definitionName) as Class;
                var display:DisplayObject = new displayClass() as DisplayObject;
                if (!display)
                    return null;
                var holder:Sprite = new Sprite();
                holder.mouseEnabled = false;
                holder.mouseChildren = false;
                holder.addChild(display);
                var maxDim:Number = Math.max(display.width, display.height);
                if (maxDim > 0.0) {
                    var iconScale:Number = 16.0 / maxDim;
                    display.scaleX = iconScale;
                    display.scaleY = iconScale;
                }
                display.transform.colorTransform = new ColorTransform(1, 1, 1, 1, 0, 0, 0, 0);
                return holder;
            } catch (error:Error) {
                return null;
            }
            return null;
        }

        private function makeText(value:String, xPos:Number, yPos:Number, size:Number, color:uint, bold:Boolean):TextField
        {
            var field:TextField = new TextField();
            field.x = xPos; field.y = yPos; field.text = value;
            field.defaultTextFormat = new TextFormat(bold ? "$MAIN_Font_Bold" : "$MAIN_Font", size, color, bold);
            field.setTextFormat(field.defaultTextFormat);
            field.selectable = false; field.mouseEnabled = false;
            field.multiline = false; field.wordWrap = false; field.embedFonts = false;
            return field;
        }

        private function setTextColor(field:TextField, color:uint):void
        {
            if (!field)
                return;
            var format:TextFormat = field.defaultTextFormat;
            format.color = color; field.defaultTextFormat = format; field.setTextFormat(format);
        }

        private function setIconColor(icon:Sprite, color:uint):void
        {
            if (!icon)
                return;
            icon.transform.colorTransform = new ColorTransform(
                ((color >> 16) & 0xFF) / 255.0,
                ((color >> 8) & 0xFF) / 255.0,
                (color & 0xFF) / 255.0,
                1, 0, 0, 0, 0);
        }

        private function numberValue(owner:Object, key:String, fallback:Number):Number
        {
            return owner && owner[key] is Number ? Number(owner[key]) : fallback;
        }

        private function themeComponent(key:String):Number
        {
            return theme && theme[key] is Number ? Math.max(0, Math.min(1, Number(theme[key]))) : 0.5;
        }

        private function themeColor(scale:Number):uint
        {
            var r:int = Math.max(0, Math.min(255, int(themeComponent("r") * 255 * scale)));
            var g:int = Math.max(0, Math.min(255, int(themeComponent("g") * 255 * scale)));
            var b:int = Math.max(0, Math.min(255, int(themeComponent("b") * 255 * scale)));
            return uint((r << 16) | (g << 8) | b);
        }

        private function updateFocusColor():void
        {
            var tr:Number = themeComponent("r");
            var tg:Number = themeComponent("g");
            var tb:Number = themeComponent("b");
            var yellowHud:Boolean = tr > 0.55 && tg > 0.42 && tb < 0.42 && tr >= tg * 0.78;
            var candidates:Array = [0xFFC857, 0x65E7FF, 0xD5A5FF, 0xFFFFFF];
            var best:uint = 0xFFC857;
            var bestScore:Number = -1000.0;
            for each (var candidate:uint in candidates) {
                var cr:Number = ((candidate >> 16) & 0xFF) / 255.0;
                var cg:Number = ((candidate >> 8) & 0xFF) / 255.0;
                var cb:Number = (candidate & 0xFF) / 255.0;
                var distance:Number = Math.sqrt(
                    (cr - tr) * (cr - tr) + (cg - tg) * (cg - tg) + (cb - tb) * (cb - tb));
                var themeLum:Number = 0.299 * tr + 0.587 * tg + 0.114 * tb;
                var candidateLum:Number = 0.299 * cr + 0.587 * cg + 0.114 * cb;
                var score:Number = distance * 0.72 + Math.abs(candidateLum - themeLum) * 0.28;
                if (yellowHud && candidate == 0xFFC857)
                    score -= 1.0;
                if (score > bestScore) {
                    bestScore = score;
                    best = candidate;
                }
            }
            focusColor = best;
            updateEquippedColor();
        }

        private function colorDistance(first:uint, second:uint):Number
        {
            var red:Number = (((first >> 16) & 0xFF) - ((second >> 16) & 0xFF)) / 255.0;
            var green:Number = (((first >> 8) & 0xFF) - ((second >> 8) & 0xFF)) / 255.0;
            var blue:Number = ((first & 0xFF) - (second & 0xFF)) / 255.0;
            return Math.sqrt(red * red + green * green + blue * blue);
        }

        private function updateEquippedColor():void
        {
            // A cool secondary accent is preferred, but it must remain
            // distinct from both the HUD color and the current focus color.
            var candidates:Array = [0x65E7FF, 0x70FFB0, 0xD5A5FF, 0xF4F0D8, 0xFFC857];
            var preference:Array = [0.16, 0.10, 0.08, 0.04, 0.0];
            var hudColor:uint = themeColor(1.0);
            var best:uint = candidates[0];
            var bestScore:Number = -1000.0;
            for (var i:int = 0; i < candidates.length; ++i) {
                var candidate:uint = uint(candidates[i]);
                var score:Number = colorDistance(candidate, hudColor) * 0.42 +
                    colorDistance(candidate, focusColor) * 0.58 + preference[i];
                if (colorDistance(candidate, hudColor) < 0.22)
                    score -= 0.45;
                if (colorDistance(candidate, focusColor) < 0.30)
                    score -= 0.75;
                if (score > bestScore) {
                    bestScore = score;
                    best = candidate;
                }
            }
            equippedColor = best;
        }

        private function clampInt(value:int, min:int, max:int):int
        {
            return Math.max(min, Math.min(max, value));
        }

        private function trimName(value:String):String
        {
            return value.length > 18 ? value.substr(0, 17) + "…" : value;
        }

        private function cleanName(value:String):String
        {
            value = value.replace(/^\s*(\[[^\]]+\]\s*)+/, "");
            if (!showItemInnerName) {
                var separator:int = value.indexOf("|");
                if (separator >= 0)
                    value = value.substr(0, separator);
            }
            value = value.replace(/\s+$/, "");
            return value.length > 0 ? value : "$AOZORA_UNKNOWN_ITEM";
        }

        private function hotkeyLabel(slot:int):String
        {
            if (slot < 0 || slot >= 12) return "$AOZORA_HOTKEY_EMPTY";
            if (slot < 9) return String(slot + 1);
            if (slot == 9) return "0";
            return slot == 10 ? "-" : "=";
        }

        private function currentFocusSignatureFor(source:Array):String
        {
            if (!source || source.length == 0 || selectedIndex < 0 || selectedIndex >= source.length)
                return "";
            var item:Object = source[selectedIndex];
            return String(item && (item.formID is Number) ? item.formID : "") + ":" +
                String(item && item.instanceKey ? item.instanceKey : "");
        }

        private function strokeRect(g:Graphics, xPos:Number, yPos:Number, width:Number, height:Number):void
        {
            g.moveTo(xPos, yPos);
            g.lineTo(xPos + width, yPos);
            g.lineTo(xPos + width, yPos + height);
            g.lineTo(xPos, yPos + height);
            g.lineTo(xPos, yPos);
        }

        private function strokeKeyCap(g:Graphics, xPos:Number, yPos:Number,
            width:Number, height:Number, cut:Number):void
        {
            // Continuous rectangular keycap; the key label remains a real
            // centered text field inside this fixed graphic.
            g.drawRect(xPos, yPos, width, height);
        }

        private function fillRect(g:Graphics, color:uint, alphaValue:Number,
            xPos:Number, yPos:Number, width:Number, height:Number):void
        {
            g.beginFill(color, alphaValue);
            strokeRect(g, xPos, yPos, width, height);
            g.endFill();
        }

    }
}
