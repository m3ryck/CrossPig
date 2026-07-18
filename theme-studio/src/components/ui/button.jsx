import { cva } from "class-variance-authority";
import { cn } from "../../lib/utils";

const variants = cva("ui-button", {
  variants: {
    variant: {
      default: "button-default",
      outline: "button-outline",
      ghost: "button-ghost",
    },
    size: { default: "button-md", sm: "button-sm" },
  },
  defaultVariants: { variant: "default", size: "default" },
});
export function Button({ className, variant, size, ...props }) {
  return (
    <button className={cn(variants({ variant, size }), className)} {...props} />
  );
}
