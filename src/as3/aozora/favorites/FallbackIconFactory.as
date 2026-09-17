package aozora.favorites
{
    import flash.display.Sprite;
    import flash.geom.ColorTransform;
    import flash.geom.Rectangle;

    /**
     * Fallback icon factory backed directly by the 12 SVG files supplied in
     * v1n/图标. Flex embeds these SVGs as native SpriteAsset vector symbols;
     * no PNG, runtime loader or external icon-library dependency is used.
     */
    public final class FallbackIconFactory
    {
        [Embed(source="../../../../图标/pistol.svg")]
        private static const PistolIcon:Class;
        [Embed(source="../../../../图标/rifle.svg")]
        private static const RifleIcon:Class;
        [Embed(source="../../../../图标/heavy .svg")]
        private static const HeavyIcon:Class;
        [Embed(source="../../../../图标/melee.svg")]
        private static const MeleeIcon:Class;
        [Embed(source="../../../../图标/explosive.svg")]
        private static const ExplosiveIcon:Class;
        [Embed(source="../../../../图标/clothing .svg")]
        private static const ClothingIcon:Class;
        [Embed(source="../../../../图标/armor.svg")]
        private static const ArmorIcon:Class;
        [Embed(source="../../../../图标/headwear .svg")]
        private static const HeadwearIcon:Class;
        [Embed(source="../../../../图标/footwear .svg")]
        private static const FootwearIcon:Class;
        [Embed(source="../../../../图标/Medicine.svg")]
        private static const MedicineIcon:Class;
        [Embed(source="../../../../图标/FoodDrink.svg")]
        private static const FoodDrinkIcon:Class;
        [Embed(source="../../../../图标/Utility.svg")]
        private static const UtilityIcon:Class;

        public static function create(type:String, value:String):Sprite
        {
            var holder:Sprite = new Sprite();
            var iconClass:Class = classForCategory(normalize(type, value));
            var vector:Sprite = new iconClass() as Sprite;
            holder.mouseEnabled = false;
            holder.mouseChildren = false;
            if (!vector)
                return holder;

            // The supplied SVGs use black as their source ink. Convert that
            // source ink to white once; the existing parent ColorTransform in
            // SkyuiLikeMenuPanel then applies normal/focus/equipped colors.
            vector.transform.colorTransform = new ColorTransform(0, 0, 0, 1, 255, 255, 255, 0);
            var bounds:Rectangle = vector.getBounds(vector);
            var maxDim:Number = Math.max(bounds.width, bounds.height);
            if (maxDim > 0.0) {
                var iconScale:Number = 16.0 / maxDim;
                vector.scaleX = iconScale;
                vector.scaleY = iconScale;
                vector.x = (16.0 - bounds.width * iconScale) / 2.0 - bounds.x * iconScale;
                vector.y = (16.0 - bounds.height * iconScale) / 2.0 - bounds.y * iconScale;
            }
            holder.addChild(vector);
            return holder;
        }

        private static function classForCategory(category:String):Class
        {
            if (category == "Pistol") return PistolIcon;
            if (category == "Rifle") return RifleIcon;
            if (category == "Heavy") return HeavyIcon;
            if (category == "Melee") return MeleeIcon;
            if (category == "Explosive") return ExplosiveIcon;
            if (category == "Clothing") return ClothingIcon;
            if (category == "Armor") return ArmorIcon;
            if (category == "Headwear") return HeadwearIcon;
            if (category == "Footwear") return FootwearIcon;
            if (category == "Medicine") return MedicineIcon;
            if (category == "FoodDrink") return FoodDrinkIcon;
            return UtilityIcon;
        }

        private static function normalize(type:String, value:String):String
        {
            var category:String = value ? value.toLowerCase() : "";
            if (!category.length) {
                return type == "服装" ? "Clothing" :
                    (type == "药品" ? "Medicine" : (type == "武器" ? "Rifle" : "Utility"));
            }
            if (category == "pistol") return "Pistol";
            if (category == "rifle" || category == "shotgun" || category == "ranged") return "Rifle";
            if (category == "heavy") return "Heavy";
            if (category == "melee") return "Melee";
            if (category == "explosive" || category == "throwable") return "Explosive";
            if (category == "clothing" || category == "outfit") return "Clothing";
            if (category == "armor") return "Armor";
            if (category == "headwear") return "Headwear";
            if (category == "footwear") return "Footwear";
            if (category == "medicine" || category == "aid") return "Medicine";
            if (category == "fooddrink" || category == "food") return "FoodDrink";
            return "Utility";
        }
    }
}
