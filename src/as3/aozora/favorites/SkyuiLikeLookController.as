package aozora.favorites
{
    import flash.display.Bitmap;
    import flash.display.BitmapData;
    import flash.display.Loader;
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.events.IOErrorEvent;
    import flash.filters.ColorMatrixFilter;
    import flash.geom.ColorTransform;
    import flash.net.URLRequest;

    /**
     * Loads one of the five supplied mascot series for a menu session.
     * A series contains ten complete poses. The pose changes only after a real
     * selection change, so browsing does not create an autonomous animation.
     */
    public class SkyuiLikeLookController extends MovieClip
    {
        public var onReady:Function;
        public var onError:Function;

        private static const GROUP_KEYS:Array = ["chat", "snack", "mechanic", "explorer", "groom"];
        private static const GROUP_LABELS:Array = ["Chat", "Snack", "Mechanic", "Explorer", "Groom"];
        // The source packs have different authored exposure. Grade the five
        // packs into the same readable range before showing them in one menu.
        // Normalize the authored packs to a common exposure and bring back
        // a little of the turquoise/brown saturation lost in the DDS pass.
        private static const GROUP_GRADES:Array = [1.02, 1.02, 1.04, 1.02, 1.03];
        private static const GROUP_SATURATION:Array = [1.10, 1.12, 1.10, 1.08, 1.10];
        private static const GROUP_CONTRAST:Array = [1.03, 1.03, 1.04, 1.03, 1.03];
        private static var lastGroup:int = -1;

        private var imageBridge:Object;
        private var specs:Array = [];
        private var loaders:Array = [];
        private var loadedData:Object = {};
        private var frameIds:Array = [];
        private var loadedCount:int = 0;
        private var ready:Boolean = false;
        private var notified:Boolean = false;
        private var failed:Boolean = false;
        private var currentIndex:int = 0;
        private var pendingAdvances:int = 0;
        private var selectedGroup:int = 0;
        private var lookBitmap:Bitmap;

        public function SkyuiLikeLookController()
        {
            super();
            mouseEnabled = false;
            mouseChildren = false;
            visible = false;
            selectedGroup = chooseGroup();
            buildSpecs();
            lookBitmap = new Bitmap();
            lookBitmap.smoothing = true;
            addChild(lookBitmap);
        }

        public function get groupName():String
        {
            return GROUP_LABELS[selectedGroup];
        }

        private function chooseGroup():int
        {
            var group:int = int(Math.floor(Math.random() * GROUP_KEYS.length));
            if (GROUP_KEYS.length > 1 && group == lastGroup)
                group = (group + 1) % GROUP_KEYS.length;
            lastGroup = group;
            return group;
        }

        private function buildSpecs():void
        {
            var key:String = String(GROUP_KEYS[selectedGroup]);
            var label:String = String(GROUP_LABELS[selectedGroup]);
            for (var i:int = 1; i <= 10; ++i) {
                var number:String = i < 10 ? "0" + i : "10";
                var id:String = "AozoraFavoritesSkyuiLike" + label + number;
                specs.push({
                    file: "skyui_like_look_" + key + "_" + number + ".dds",
                    id: id
                });
                frameIds.push(id);
            }
        }

        public function attachImageBridge(codeObject:Object):void
        {
            if (!codeObject || !(codeObject.MountImage is Function)) {
                reportError("LOOK:no-bridge");
                return;
            }
            imageBridge = codeObject;
            loaders = [];
            loadedData = {};
            loadedCount = 0;
            ready = false;
            notified = false;
            failed = false;
            currentIndex = 0;
            pendingAdvances = 0;
            for each (var spec:Object in specs) {
                try {
                    var mounted:Boolean = Boolean(imageBridge.MountImage(
                        "AozoraFavoritesMenu",
                        "Interface\\AozoraFavorites\\" + spec.file,
                        spec.id));
                    if (!mounted) {
                        failed = true;
                        reportError("LOOK:mount-" + spec.file);
                        continue;
                    }
                    var loader:Loader = new Loader();
                    loader.name = String(spec.id);
                    loader.contentLoaderInfo.addEventListener(Event.COMPLETE, onLoaded);
                    loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, onLoadError);
                    loaders.push(loader);
                    loader.load(new URLRequest("img://" + spec.id));
                } catch (error:Error) {
                    failed = true;
                    reportError("LOOK:error-" + spec.file);
                }
            }
            if (loaders.length == 0)
                reportError("LOOK:no-assets");
        }

        private function onLoaded(event:Event):void
        {
            var info:Object = event.currentTarget;
            var loader:Loader = info && info.loader is Loader ? info.loader as Loader : null;
            if (!loader)
                return;
            var bitmap:Bitmap = loader.content as Bitmap;
            if (!bitmap || !bitmap.bitmapData) {
                onLoadError(null);
                return;
            }
            loadedData[loader.name] = bitmap.bitmapData;
            ++loadedCount;
            if (loadedCount >= specs.length && !failed)
                finishReady();
        }

        private function onLoadError(event:IOErrorEvent):void
        {
            failed = true;
            reportError("LOOK:load-" + groupName);
        }

        private function finishReady():void
        {
            ready = true;
            setLookBitmap(frameIds[currentIndex]);
            while (pendingAdvances > 0) {
                currentIndex = (currentIndex + 1) % frameIds.length;
                --pendingAdvances;
            }
            setLookBitmap(frameIds[currentIndex]);
            if (!notified && onReady != null) {
                notified = true;
                onReady(this);
            }
        }

        private function setLookBitmap(id:String):void
        {
            var data:BitmapData = loadedData[id] as BitmapData;
            if (!data || !lookBitmap)
                return;
            lookBitmap.bitmapData = data;
            lookBitmap.visible = true;
            lookBitmap.alpha = 1.0;
            var grade:Number = Number(GROUP_GRADES[selectedGroup]);
            var saturation:Number = Number(GROUP_SATURATION[selectedGroup]);
            var contrast:Number = Number(GROUP_CONTRAST[selectedGroup]);
            lookBitmap.transform.colorTransform = new ColorTransform();
            lookBitmap.filters = [new ColorMatrixFilter(buildColorMatrix(grade, saturation, contrast))];
            lookBitmap.x = 0;
            lookBitmap.y = 0;
            // Mascot DDS assets are 1024x1536 for quality, while the prior
            // runtime assets were 512x768. Keep the same logical footprint.
            lookBitmap.scaleX = 0.5;
            lookBitmap.scaleY = 0.5;
        }

        private function buildColorMatrix(brightness:Number, saturation:Number, contrast:Number):Array
        {
            var inv:Number = 1.0 - saturation;
            var lumR:Number = 0.2126 * inv;
            var lumG:Number = 0.7152 * inv;
            var lumB:Number = 0.0722 * inv;
            return [
                (lumR + saturation) * contrast * brightness, lumG * contrast * brightness, lumB * contrast * brightness, 0, 0,
                lumR * contrast * brightness, (lumG + saturation) * contrast * brightness, lumB * contrast * brightness, 0, 0,
                lumR * contrast * brightness, lumG * contrast * brightness, (lumB + saturation) * contrast * brightness, 0, 0,
                0, 0, 0, 1, 0
            ];
        }

        public function onFocusChanged():void
        {
            if (frameIds.length == 0)
                return;
            if (!ready) {
                if (pendingAdvances < frameIds.length)
                    ++pendingAdvances;
                return;
            }
            currentIndex = (currentIndex + 1) % frameIds.length;
            setLookBitmap(frameIds[currentIndex]);
        }

        private function reportError(message:String):void
        {
            if (onError != null)
                onError(message);
        }
    }
}
