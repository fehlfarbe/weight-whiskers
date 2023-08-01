import { useDescription, useTsController, createTsForm } from "@ts-react/form"
import { z } from "zod"

function TextField() {
    const { field } = useTsController<string>()
    // to render the label and placeholder
    const { label, placeholder } = useDescription()

    return (
        <>
            <label>{label}</label>
            <input
                placeholder={placeholder}
                className="input-group fluid" 
                value={field.value ? field.value : ""} 
                onChange={(e) => {
                    // to update the form field associated to this
                    // input component 
                    field.onChange(e.target.value)
                }}
            />
        </>
    )
}

function NumberField() {
    const { field } = useTsController<string>()
    // to render the label and placeholder
    const { label, placeholder } = useDescription()

    return (
        <>
            <label>{label}</label>
            <input
                placeholder={placeholder}
                className="input-group fluid" 
                type="number"
                value={field.value ? field.value : ""} 
                onChange={(e) => {
                    // to update the form field associated to this
                    // input component 
                    field.onChange(e.target.value)
                }}
            />
        </>
    )
}

// specify the mapping between the zod types
// and your input components
const mapping = [
    [z.string(), TextField],
    [z.boolean(), TextField],
    [z.number(), NumberField],
    [z.date(), TextField],
] as const // <- note that this "as const" is necessary

// create the type-safe form React component
const TypeSafeForm = createTsForm(mapping)

export default TypeSafeForm