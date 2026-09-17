package aozora.favorites
{
    import flash.display.MovieClip;
    import flash.events.Event;

    public class AozoraFavoritesMenu extends MovieClip
    {
        public var BGSCodeObj:Object;
        public var Menu_mc:SkyuiLikeMenuPanel;

        public function AozoraFavoritesMenu()
        {
            super();
            BGSCodeObj = new Object();
            Menu_mc = new SkyuiLikeMenuPanel();
            Menu_mc.name = "Menu_mc";
            Menu_mc.BGSCodeObj = BGSCodeObj;
            BGSCodeObj.ProcessUserEvent = Menu_mc.ProcessUserEvent;
            addChild(Menu_mc);
            addEventListener(Event.ENTER_FRAME, initializeMenu);
        }

        private function initializeMenu(event:Event):void
        {
            removeEventListener(Event.ENTER_FRAME, initializeMenu);
            Menu_mc.InitializeAfterNativeBinding();
        }

        // F4SE calls this hook for every Scaleform movie after installing its
        // global code object. The menu uses it to mount Bethesda textures via
        // img:// instead of relying on unsupported embedded PNG decoding.
        public function onF4SEObjCreated(codeObject:Object):void
        {
            if (Menu_mc)
                Menu_mc.OnF4SEObjCreated(codeObject);
        }
    }
}
